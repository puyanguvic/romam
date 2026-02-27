#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.common import SUPPORTED_PROTOCOLS, SUPPORTED_PROTOCOLS_SET


MODE_AUTO = "auto"
MODE_SCENARIO = "scenario"
MODE_BENCHMARK_RUN = "benchmark_run"
MODE_BENCHMARK_SUMMARY = "benchmark_summary"
MODE_EXP1_SUMMARY = "exp1_summary"
ALL_MODES = (
    MODE_AUTO,
    MODE_SCENARIO,
    MODE_BENCHMARK_RUN,
    MODE_BENCHMARK_SUMMARY,
    MODE_EXP1_SUMMARY,
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Validate unified experiment JSON outputs. "
            "Supports scenario report, benchmark per-run report, benchmark summary, "
            "and Exp1 protocol-matrix summary."
        )
    )
    parser.add_argument(
        "--input",
        required=True,
        help="Path to a JSON file or a directory containing JSON files.",
    )
    parser.add_argument(
        "--mode",
        choices=ALL_MODES,
        default=MODE_AUTO,
        help="Validation mode (default: auto-detect from JSON keys).",
    )
    parser.add_argument(
        "--recursive",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Recursively scan directories for *.json files (default: enabled).",
    )
    parser.add_argument(
        "--fail-fast",
        action="store_true",
        help="Stop at the first validation failure.",
    )
    return parser.parse_args()


def _require_keys(obj: dict[str, Any], keys: list[str], *, prefix: str = "") -> list[str]:
    missing = [f"{prefix}{key}" for key in keys if key not in obj]
    return missing


def _is_number(value: Any) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def detect_mode(payload: dict[str, Any]) -> str:
    if payload.get("experiment") == "exp1_protocol_functionality_abilene" and isinstance(
        payload.get("rows"), list
    ):
        return MODE_EXP1_SUMMARY
    if payload.get("experiment") == "unified_convergence_benchmark" and isinstance(
        payload.get("runs"), list
    ):
        return MODE_BENCHMARK_SUMMARY
    if isinstance(payload.get("probes"), list) and isinstance(payload.get("run_id"), str):
        return MODE_BENCHMARK_RUN
    if isinstance(payload.get("samples"), list) and isinstance(payload.get("apps"), dict):
        return MODE_SCENARIO
    return ""


def validate_scenario(payload: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    errors.extend(
        _require_keys(
            payload,
            [
                "config_file",
                "lab_name",
                "protocol",
                "topology_file",
                "duration_s",
                "poll_interval_s",
                "traffic",
                "apps",
                "convergence",
                "samples",
            ],
        )
    )
    if errors:
        return errors
    if payload.get("protocol") not in SUPPORTED_PROTOCOLS_SET:
        errors.append("protocol must be one of: " + ", ".join(SUPPORTED_PROTOCOLS))
    if not _is_number(payload.get("duration_s")):
        errors.append("duration_s must be a number")
    if not _is_number(payload.get("poll_interval_s")):
        errors.append("poll_interval_s must be a number")
    apps = payload.get("apps")
    if not isinstance(apps, dict):
        errors.append("apps must be an object")
    else:
        errors.extend(_require_keys(apps, ["mode", "count", "specs"], prefix="apps."))
    conv = payload.get("convergence")
    if not isinstance(conv, dict):
        errors.append("convergence must be an object")
    else:
        errors.extend(
            _require_keys(
                conv,
                ["initial_converged_at_s", "fault_reconvergence"],
                prefix="convergence.",
            )
        )
    samples = payload.get("samples")
    if not isinstance(samples, list):
        errors.append("samples must be a list")
    elif samples:
        first = samples[0]
        if not isinstance(first, dict):
            errors.append("samples[0] must be an object")
        else:
            errors.extend(_require_keys(first, ["t_s", "node_data"], prefix="samples[0]."))
    return errors


def validate_benchmark_run(payload: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    errors.extend(
        _require_keys(
            payload,
            [
                "run_id",
                "name",
                "run_idx",
                "protocol",
                "n_nodes",
                "topology",
                "successful_probes",
                "failed_probes",
                "probe_success_ratio",
                "converged",
                "probes",
            ],
        )
    )
    if errors:
        return errors
    if payload.get("protocol") not in SUPPORTED_PROTOCOLS_SET:
        errors.append("protocol must be one of: " + ", ".join(SUPPORTED_PROTOCOLS))
    if not isinstance(payload.get("probes"), list):
        errors.append("probes must be a list")
    return errors


def validate_benchmark_summary(payload: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    errors.extend(
        _require_keys(
            payload,
            [
                "experiment",
                "mode",
                "protocol",
                "repeats",
                "runs",
                "avg_probe_success_ratio",
                "convergence_success_rate",
            ],
        )
    )
    if errors:
        return errors
    if payload.get("experiment") != "unified_convergence_benchmark":
        errors.append("experiment must be unified_convergence_benchmark")
    if payload.get("mode") != "convergence_benchmark":
        errors.append("mode must be convergence_benchmark")
    if payload.get("protocol") not in SUPPORTED_PROTOCOLS_SET:
        errors.append("protocol must be one of: " + ", ".join(SUPPORTED_PROTOCOLS))
    runs = payload.get("runs")
    if not isinstance(runs, list):
        errors.append("runs must be a list")
    return errors


def validate_exp1_summary(payload: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    errors.extend(
        _require_keys(
            payload,
            [
                "experiment",
                "created_at_utc",
                "protocols",
                "rows",
                "route_snapshots",
            ],
        )
    )
    if errors:
        return errors

    if payload.get("experiment") != "exp1_protocol_functionality_abilene":
        errors.append("experiment must be exp1_protocol_functionality_abilene")

    protocols = payload.get("protocols")
    if not isinstance(protocols, list):
        errors.append("protocols must be a list")
        protocols = []
    else:
        unknown = [item for item in protocols if item not in SUPPORTED_PROTOCOLS_SET]
        if unknown:
            errors.append(
                "protocols contains unsupported entries: "
                + ", ".join(str(item) for item in unknown)
            )

    rows = payload.get("rows")
    if not isinstance(rows, list):
        errors.append("rows must be a list")
    else:
        for idx, row in enumerate(rows):
            if not isinstance(row, dict):
                errors.append(f"rows[{idx}] must be an object")
                continue
            errors.extend(_require_keys(row, ["protocol"], prefix=f"rows[{idx}]."))
            row_protocol = row.get("protocol")
            if row_protocol not in SUPPORTED_PROTOCOLS_SET:
                errors.append(
                    f"rows[{idx}].protocol must be one of: " + ", ".join(SUPPORTED_PROTOCOLS)
                )

    snapshots = payload.get("route_snapshots")
    if not isinstance(snapshots, dict):
        errors.append("route_snapshots must be an object")
    elif isinstance(protocols, list):
        missing_snapshot = [name for name in protocols if name not in snapshots]
        if missing_snapshot:
            errors.append(
                "route_snapshots missing protocols: " + ", ".join(missing_snapshot)
            )

    return errors


def validate_payload(payload: dict[str, Any], mode: str) -> tuple[str, list[str]]:
    resolved_mode = mode
    if mode == MODE_AUTO:
        resolved_mode = detect_mode(payload)
        if not resolved_mode:
            return ("", ["unable to auto-detect mode from JSON keys"])
    if resolved_mode == MODE_SCENARIO:
        return (resolved_mode, validate_scenario(payload))
    if resolved_mode == MODE_BENCHMARK_RUN:
        return (resolved_mode, validate_benchmark_run(payload))
    if resolved_mode == MODE_BENCHMARK_SUMMARY:
        return (resolved_mode, validate_benchmark_summary(payload))
    if resolved_mode == MODE_EXP1_SUMMARY:
        return (resolved_mode, validate_exp1_summary(payload))
    return (resolved_mode, [f"unsupported mode: {resolved_mode}"])


def discover_json_files(path: Path, recursive: bool) -> list[Path]:
    if path.is_file():
        return [path]
    if not path.is_dir():
        return []
    if recursive:
        return sorted([p for p in path.rglob("*.json") if p.is_file()])
    return sorted([p for p in path.glob("*.json") if p.is_file()])


def main() -> int:
    args = parse_args()
    in_path = Path(str(args.input)).expanduser().resolve()
    files = discover_json_files(in_path, recursive=bool(args.recursive))
    if not files:
        print(f"[error] no JSON file found under: {in_path}")
        return 1

    failures = 0
    for path in files:
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except Exception as exc:  # noqa: BLE001
            failures += 1
            print(f"[invalid] {path}: cannot parse JSON ({exc})")
            if args.fail_fast:
                return 1
            continue
        if not isinstance(payload, dict):
            failures += 1
            print(f"[invalid] {path}: top-level JSON must be an object")
            if args.fail_fast:
                return 1
            continue

        resolved_mode, errors = validate_payload(payload, str(args.mode))
        if errors:
            failures += 1
            mode_label = resolved_mode or "unknown"
            print(f"[invalid] {path} ({mode_label})")
            for err in errors:
                print(f"  - {err}")
            if args.fail_fast:
                return 1
            continue
        print(f"[ok] {path} ({resolved_mode})")

    print(f"validated={len(files)} failures={failures}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
