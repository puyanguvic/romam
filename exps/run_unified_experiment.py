#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any

import yaml

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from irp.utils.io import dump_json, ensure_dir, now_tag
from topology.clab_loader import ClabTopology, load_clab_topology

KNOWN_KEYS = ("topology_file", "configs_dir", "deploy_env_file", "lab_name")
TOPOLOGY_ALIAS = {
    "line_3": "line3",
    "line3": "line3",
    "line_5": "line5",
    "line5": "line5",
    "ring_6": "ring6",
    "ring6": "ring6",
}


@dataclass(frozen=True)
class FaultSpec:
    time_s: float
    type: str
    data: dict[str, Any]


@dataclass(frozen=True)
class AppSpec:
    name: str
    node: str
    kind: str
    bin: str
    args: list[str]
    env: dict[str, str]
    restart: str
    max_restarts: int
    delay_s: float
    log_file: str
    cpu_affinity: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Run unified routing experiment from a single YAML config: "
            "deploy, launch apps, inject faults, and sample /v1/routes+/v1/metrics every second."
        )
    )
    parser.add_argument("--config", required=True, help="Path to unified experiment YAML.")
    parser.add_argument(
        "--output-json",
        default="",
        help="Optional output report path. Default under results/runs/unified_experiments/.",
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for docker/containerlab commands (default: enabled).",
    )
    parser.add_argument(
        "--poll-interval-s",
        type=float,
        default=1.0,
        help="Sampling interval for management APIs and app supervision.",
    )
    parser.add_argument(
        "--keep-lab",
        action="store_true",
        help="Keep lab after experiment (skip destroy).",
    )
    return parser.parse_args()


def run_cmd(
    cmd: list[str],
    *,
    check: bool = True,
    capture_output: bool = True,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(
        cmd,
        check=False,
        text=True,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.STDOUT if capture_output else None,
        env=env,
    )
    if check and proc.returncode != 0:
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{(proc.stdout or '').strip()}"
        )
    return proc


def parse_keyed_output(text: str) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in text.splitlines():
        stripped = line.strip()
        for key in KNOWN_KEYS:
            prefix = f"{key}:"
            if stripped.startswith(prefix):
                out[key] = stripped[len(prefix) :].strip()
    return out


def load_env_file(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.strip()
        if not text or text.startswith("#") or "=" not in text:
            continue
        key, value = text.split("=", maxsplit=1)
        out[key.strip()] = value.strip()
    return out


def resolve_topology_spec(spec: str) -> tuple[str, str]:
    text = str(spec).strip()
    if not text:
        raise ValueError("topology is required")

    candidate = Path(text).expanduser()
    if candidate.is_absolute() and candidate.exists():
        return ("topology_file", str(candidate))

    rel_candidate = (REPO_ROOT / candidate).resolve()
    if rel_candidate.exists() and rel_candidate.suffix in {".yaml", ".yml"}:
        return ("topology_file", str(rel_candidate))

    normalized = TOPOLOGY_ALIAS.get(text.lower(), text.lower())
    builtin = (REPO_ROOT / "clab_topologies" / f"{normalized}.clab.yaml").resolve()
    if builtin.exists():
        return ("profile", normalized)

    raise ValueError(
        f"unsupported topology spec '{spec}'. Use builtin profile (e.g. line3/ring6) or file path."
    )


def with_sudo(cmd: list[str], use_sudo: bool) -> list[str]:
    return ["sudo", *cmd] if use_sudo else cmd


def run_containerlab_destroy(
    *,
    topology_file: Path,
    lab_name: str,
    deploy_env: dict[str, str],
    use_sudo: bool,
) -> None:
    clab_bin = shutil.which("containerlab")
    if clab_bin is None:
        raise RuntimeError("containerlab not found in PATH")

    destroy_cmd = [
        clab_bin,
        "destroy",
        "-t",
        str(topology_file),
        "--name",
        str(lab_name),
        "--cleanup",
    ]
    if use_sudo:
        exports = [f"{k}={v}" for k, v in deploy_env.items()]
        run_cmd(["sudo", "env", *exports, *destroy_cmd], check=False, capture_output=False)
        return
    run_cmd(
        destroy_cmd,
        check=False,
        capture_output=False,
        env={**os.environ, **deploy_env},
    )


def choose_end_nodes(topo: ClabTopology) -> tuple[str, str]:
    degree: dict[str, int] = {name: 0 for name in topo.node_names}
    for link in topo.links:
        degree[link.left_node] = degree.get(link.left_node, 0) + 1
        degree[link.right_node] = degree.get(link.right_node, 0) + 1

    ends = [name for name in topo.node_names if degree.get(name, 0) == 1]
    if len(ends) == 2:
        return str(ends[0]), str(ends[1])
    if len(topo.node_names) >= 2:
        return str(topo.node_names[0]), str(topo.node_names[-1])
    raise RuntimeError("topology must include at least 2 nodes")


def choose_node_data_ip(topo: ClabTopology, node_name: str) -> str:
    for link in topo.links:
        if link.left_node == node_name and link.left_ip:
            return str(link.left_ip)
        if link.right_node == node_name and link.right_ip:
            return str(link.right_ip)
    raise RuntimeError(f"no data-plane ip found for node={node_name}")


def load_management_http_port(config_dir: Path, node_name: str) -> int:
    cfg_path = config_dir / f"{node_name}.yaml"
    with cfg_path.open("r", encoding="utf-8") as f:
        cfg = yaml.safe_load(f) or {}
    try:
        return int(cfg.get("management", {}).get("http", {}).get("port", 0))
    except (TypeError, ValueError):
        return 0


def container_name(lab_name: str, node_name: str) -> str:
    return f"clab-{lab_name}-{node_name}"


def fetch_mgmt_json(*, use_sudo: bool, container: str, port: int, path: str) -> dict[str, Any]:
    if port <= 0:
        return {}
    text = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                (
                    f"wget -qO- http://127.0.0.1:{port}{path} 2>/dev/null "
                    f"|| curl -fsS http://127.0.0.1:{port}{path} 2>/dev/null "
                    "|| true"
                ),
            ],
            use_sudo,
        ),
        check=False,
    ).stdout or ""
    payload = text.strip()
    if not payload:
        return {}
    try:
        data = json.loads(payload)
        if isinstance(data, dict):
            return data
        return {"value": data}
    except json.JSONDecodeError:
        return {"error": "non_json", "raw": payload[:400]}


def fetch_supervisor_state(*, use_sudo: bool, container: str) -> dict[str, Any]:
    text = run_cmd(
        with_sudo(
            [
                "docker",
                "exec",
                container,
                "sh",
                "-lc",
                "cat /tmp/node_supervisor_state.json 2>/dev/null || true",
            ],
            use_sudo,
        ),
        check=False,
    ).stdout or ""
    payload = text.strip()
    if not payload:
        return {}
    try:
        data = json.loads(payload)
        if isinstance(data, dict):
            return data
        return {"value": data}
    except json.JSONDecodeError:
        return {"error": "non_json", "raw": payload[:400]}


def load_faults(raw_faults: Any) -> list[FaultSpec]:
    out: list[FaultSpec] = []
    for item in list(raw_faults or []):
        if not isinstance(item, dict):
            continue
        fault_type = str(item.get("type", "")).strip().lower()
        if not fault_type:
            continue
        try:
            t = float(item.get("time_s", 0.0))
        except (TypeError, ValueError):
            continue
        payload = {
            str(k): v for k, v in item.items() if str(k) not in {"time_s", "type"}
        }
        out.append(FaultSpec(time_s=t, type=fault_type, data=payload))
    out.sort(key=lambda x: x.time_s)
    return out


def resolve_fault_link(topo: ClabTopology, link_nodes: tuple[str, str]):
    left, right = link_nodes
    for link in topo.links:
        if (link.left_node == left and link.right_node == right) or (
            link.left_node == right and link.right_node == left
        ):
            return link
    return None


def apply_link_down_fault(
    *,
    use_sudo: bool,
    lab_name: str,
    topo: ClabTopology,
    fault: FaultSpec,
) -> dict[str, Any]:
    link_raw = list(fault.data.get("link", []) or [])
    if len(link_raw) != 2:
        return {
            "time_s": fault.time_s,
            "type": fault.type,
            "ok": False,
            "error": "link_down fault requires link: [node_a, node_b]",
        }
    left = str(link_raw[0]).strip()
    right = str(link_raw[1]).strip()
    if not left or not right:
        return {
            "time_s": fault.time_s,
            "type": fault.type,
            "ok": False,
            "error": "invalid link endpoint",
        }
    link_tuple = (left, right)

    link = resolve_fault_link(topo, link_tuple)
    if link is None:
        return {
            "time_s": fault.time_s,
            "type": fault.type,
            "link": list(link_tuple),
            "ok": False,
            "error": "link not found in topology",
        }

    steps = [
        (link.left_node, link.left_iface),
        (link.right_node, link.right_iface),
    ]
    errors: list[str] = []
    for node, iface in steps:
        cmd = with_sudo(
            [
                "docker",
                "exec",
                container_name(lab_name, node),
                "ip",
                "link",
                "set",
                "dev",
                iface,
                "down",
            ],
            use_sudo,
        )
        proc = run_cmd(cmd, check=False)
        if proc.returncode != 0:
            errors.append((proc.stdout or "").strip() or f"failed on {node}:{iface}")

    return {
        "time_s": fault.time_s,
        "type": fault.type,
        "link": list(link_tuple),
        "resolved": {
            "left": [link.left_node, link.left_iface],
            "right": [link.right_node, link.right_iface],
        },
        "ok": len(errors) == 0,
        "errors": errors,
    }


def apply_app_toggle_fault(
    *,
    use_sudo: bool,
    lab_name: str,
    config_dir: Path,
    topo: ClabTopology,
    app_specs: list[AppSpec],
    app_enabled_overrides: dict[str, bool],
    log_level: str,
    fault: FaultSpec,
    event_time_s: float,
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    if fault.type not in {"app_stop", "app_start"}:
        return (
            {
                "time_s": fault.time_s,
                "type": fault.type,
                "ok": False,
                "error": "unsupported app fault type",
            },
            [],
        )

    node = str(fault.data.get("node", fault.data.get("node_id", ""))).strip()
    app_name = str(fault.data.get("app", fault.data.get("app_id", ""))).strip()
    enable = fault.type == "app_start"
    if not app_name:
        return (
            {
                "time_s": fault.time_s,
                "type": fault.type,
                "node": node,
                "ok": False,
                "error": "app fault requires app or app_id",
            },
            [],
        )

    matches = [spec for spec in app_specs if spec.name == app_name]
    if node:
        matches = [spec for spec in matches if spec.node == node]
    if not matches:
        return (
            {
                "time_s": fault.time_s,
                "type": fault.type,
                "node": node,
                "app": app_name,
                "ok": False,
                "error": "target app not found",
            },
            [],
        )
    if len(matches) > 1:
        return (
            {
                "time_s": fault.time_s,
                "type": fault.type,
                "node": node,
                "app": app_name,
                "ok": False,
                "error": "ambiguous app target; provide node",
            },
            [],
        )

    target = matches[0]
    app_enabled_overrides[target.name] = enable
    supervisor_events = configure_node_supervisor_apps(
        use_sudo=use_sudo,
        lab_name=lab_name,
        config_dir=config_dir,
        topo=topo,
        app_specs=app_specs,
        log_level=log_level,
        enabled_overrides=app_enabled_overrides,
        only_nodes={target.node},
        event_time_s=event_time_s,
        event_name="node_supervisor_restarted_by_fault",
    )
    return (
        {
            "time_s": fault.time_s,
            "type": fault.type,
            "node": target.node,
            "app": target.name,
            "enabled": enable,
            "ok": True,
            "errors": [],
        },
        supervisor_events,
    )


def parse_flag_value(args: list[str], flag_name: str) -> str:
    for idx, token in enumerate(args):
        if token == flag_name and idx + 1 < len(args):
            return str(args[idx + 1]).strip()
        if token.startswith(f"{flag_name}="):
            return str(token.split("=", maxsplit=1)[1]).strip()
    return ""


def parse_args_list(raw: Any) -> list[str]:
    if raw is None:
        return []
    if isinstance(raw, str):
        return shlex.split(raw)
    if isinstance(raw, list):
        return [str(x) for x in raw]
    raise ValueError(f"apps[].args must be list|string, got {type(raw).__name__}")


def normalize_restart_policy(raw: Any) -> str:
    text = str(raw if raw is not None else "never").strip().lower()
    if text in {"always", "on-failure", "never", "no"}:
        return "never" if text == "no" else text
    if text in {"on_failure", "onfailure"}:
        return "on-failure"
    raise ValueError("restart must be one of: never, on-failure, always")


def parse_app_entry(raw: dict[str, Any], default_node: str, index: int) -> AppSpec:
    node = str(raw.get("node", raw.get("node_id", default_node))).strip()
    if not node:
        raise ValueError(f"apps[{index}] missing node/node_id")

    args = parse_args_list(raw.get("args", []))
    kind = str(raw.get("kind", "")).strip().lower()
    if not kind:
        if args and args[0] == "send":
            kind = "sender"
        elif args and args[0] == "sink":
            kind = "sink"
        else:
            kind = "custom"
    if kind not in {"sender", "sink", "custom"}:
        raise ValueError(f"apps[{index}] unsupported kind: {kind}")

    name = str(raw.get("name", "")).strip() or f"{node}_{kind}_{index+1}"
    bin_path = str(raw.get("bin", "/irp/bin/traffic_app")).strip() or "/irp/bin/traffic_app"

    is_traffic_app = Path(bin_path).name == "traffic_app"
    if is_traffic_app and kind in {"sender", "sink"}:
        role_token = "send" if kind == "sender" else "sink"
        if not args:
            raise ValueError(f"apps[{index}] traffic_app requires args")
        if args[0] in {"send", "sink"} and args[0] != role_token:
            raise ValueError(
                f"apps[{index}] kind={kind} conflicts with args role={args[0]}"
            )
        if args[0] not in {"send", "sink"}:
            args = [role_token, *args]

    env_raw = raw.get("env", {}) or {}
    if not isinstance(env_raw, dict):
        raise ValueError(f"apps[{index}].env must be mapping")
    env = {str(k): str(v) for k, v in env_raw.items()}
    env.setdefault("APP_ID", name)
    env.setdefault("NODE_ID", node)
    env.setdefault("APP_ROLE", kind)
    if is_traffic_app:
        env.setdefault("TRAFFIC_LOG_JSON", "1")
        if kind == "sender":
            flow_id = parse_flag_value(args, "--flow-id")
            if flow_id:
                env.setdefault("FLOW_ID", flow_id)

    restart = normalize_restart_policy(raw.get("restart", "never"))
    max_restarts_default = 0
    if restart == "always":
        max_restarts_default = -1
    elif restart == "on-failure":
        max_restarts_default = 10
    max_restarts = int(raw.get("max_restarts", max_restarts_default))

    delay_s = max(0.0, float(raw.get("delay_s", 0.0)))
    log_file = str(raw.get("log_file", f"/tmp/{name}.log")).strip() or f"/tmp/{name}.log"
    cpu_affinity = str(raw.get("cpu_affinity", "")).strip()

    return AppSpec(
        name=name,
        node=node,
        kind=kind,
        bin=bin_path,
        args=args,
        env=env,
        restart=restart,
        max_restarts=max_restarts,
        delay_s=delay_s,
        log_file=log_file,
        cpu_affinity=cpu_affinity,
    )


def infer_sink_socket(spec: AppSpec) -> tuple[str, str, int] | None:
    if spec.kind != "sink":
        return None
    if Path(spec.bin).name != "traffic_app":
        return None

    args = list(spec.args)
    if not args or args[0] != "sink":
        return None

    proto = parse_flag_value(args, "--proto") or "udp"
    bind = parse_flag_value(args, "--bind") or "0.0.0.0"
    port_raw = parse_flag_value(args, "--port")
    if not port_raw:
        return None
    try:
        port = int(port_raw)
    except ValueError:
        return None
    if port <= 0:
        return None
    return (proto, bind, port)


def infer_sender_target(spec: AppSpec) -> str:
    if spec.kind != "sender":
        return ""
    if Path(spec.bin).name != "traffic_app":
        return ""
    args = list(spec.args)
    if not args or args[0] != "send":
        return ""
    return parse_flag_value(args, "--target")


def build_legacy_traffic_apps(
    *,
    traffic_cfg: dict[str, Any],
    topo: ClabTopology,
    duration_s: float,
) -> tuple[list[AppSpec], dict[str, Any]]:
    enabled = bool(traffic_cfg.get("enabled", bool(traffic_cfg))) and float(
        traffic_cfg.get("rate_mbps", 0.0)
    ) > 0.0

    traffic_proto = str(traffic_cfg.get("proto", "udp")).strip().lower() or "udp"
    traffic_model = str(traffic_cfg.get("model", "bulk")).strip().lower() or "bulk"
    traffic_flows = max(1, int(traffic_cfg.get("flows", 1)))
    traffic_rate_mbps = max(0.0, float(traffic_cfg.get("rate_mbps", 10.0)))
    traffic_packet_size = max(64, int(traffic_cfg.get("packet_size", 512)))
    traffic_port = int(traffic_cfg.get("port", 9000))
    traffic_on_ms = float(traffic_cfg.get("on_ms", 2000.0))
    traffic_off_ms = float(traffic_cfg.get("off_ms", 1000.0))

    src_node = str(traffic_cfg.get("src_node", "")).strip()
    dst_node = str(traffic_cfg.get("dst_node", "")).strip()
    dst_ip = str(traffic_cfg.get("dst_ip", "")).strip()

    if not enabled:
        return (
            [],
            {
                "enabled": False,
                "legacy_mode": True,
                "src_node": src_node,
                "dst_node": dst_node,
                "dst_ip": dst_ip,
                "flows": traffic_flows,
            },
        )

    if not src_node or not dst_node:
        auto_src, auto_dst = choose_end_nodes(topo)
        src_node = src_node or auto_src
        dst_node = dst_node or auto_dst
    if src_node not in topo.node_names:
        raise ValueError(f"traffic.src_node not found in topology: {src_node}")
    if dst_node not in topo.node_names:
        raise ValueError(f"traffic.dst_node not found in topology: {dst_node}")
    if not dst_ip:
        dst_ip = choose_node_data_ip(topo, dst_node)

    sink_spec = AppSpec(
        name="legacy_sink",
        node=dst_node,
        kind="sink",
        bin="/irp/bin/traffic_app",
        args=[
            "sink",
            "--proto",
            traffic_proto,
            "--bind",
            "0.0.0.0",
            "--port",
            str(traffic_port),
            "--report-interval-s",
            "1",
            "--duration-s",
            str(duration_s),
        ],
        env={
            "APP_ID": "legacy_sink",
            "NODE_ID": dst_node,
            "APP_ROLE": "sink",
            "TRAFFIC_LOG_JSON": "1",
        },
        restart="never",
        max_restarts=0,
        delay_s=0.0,
        log_file="/tmp/traffic_sink.log",
        cpu_affinity="",
    )

    pps_total = traffic_rate_mbps * 1_000_000.0 / (8.0 * float(traffic_packet_size))
    pps_per_flow = max(1.0, pps_total / float(traffic_flows))

    sender_specs: list[AppSpec] = []
    for flow_idx in range(traffic_flows):
        sender_args = [
            "send",
            "--proto",
            traffic_proto,
            "--target",
            dst_ip,
            "--port",
            str(traffic_port),
            "--packet-size",
            str(traffic_packet_size),
            "--duration-s",
            str(duration_s),
            "--pps",
            f"{pps_per_flow:.3f}",
            "--report-interval-s",
            "1",
            "--flow-id",
            str(flow_idx + 1),
        ]
        if traffic_model == "onoff":
            sender_args.extend(
                [
                    "--pattern",
                    "onoff",
                    "--on-ms",
                    str(traffic_on_ms),
                    "--off-ms",
                    str(traffic_off_ms),
                ]
            )

        app_name = f"legacy_sender_{flow_idx+1}"
        sender_specs.append(
            AppSpec(
                name=app_name,
                node=src_node,
                kind="sender",
                bin="/irp/bin/traffic_app",
                args=sender_args,
                env={
                    "APP_ID": app_name,
                    "NODE_ID": src_node,
                    "APP_ROLE": "sender",
                    "FLOW_ID": str(flow_idx + 1),
                    "TRAFFIC_LOG_JSON": "1",
                },
                restart="never",
                max_restarts=0,
                delay_s=0.0,
                log_file=f"/tmp/traffic_sender_{flow_idx+1}.log",
                cpu_affinity="",
            )
        )

    return (
        [sink_spec, *sender_specs],
        {
            "enabled": True,
            "legacy_mode": True,
            "proto": traffic_proto,
            "model": traffic_model,
            "flows": traffic_flows,
            "rate_mbps": traffic_rate_mbps,
            "packet_size": traffic_packet_size,
            "port": traffic_port,
            "src_node": src_node,
            "dst_node": dst_node,
            "dst_ip": dst_ip,
            "sink_started": True,
            "sender_started": len(sender_specs),
        },
    )


def collect_explicit_apps(config: dict[str, Any]) -> list[AppSpec]:
    out: list[AppSpec] = []
    idx = 0

    for node_entry in list(config.get("node_apps", []) or []):
        if not isinstance(node_entry, dict):
            continue
        node_id = str(node_entry.get("node_id", node_entry.get("node", ""))).strip()
        for app in list(node_entry.get("apps", []) or []):
            if isinstance(app, dict):
                out.append(parse_app_entry(app, node_id, idx))
                idx += 1

    for node_entry in list(config.get("nodes", []) or []):
        if not isinstance(node_entry, dict):
            continue
        node_id = str(node_entry.get("node_id", node_entry.get("node", ""))).strip()
        apps = list(node_entry.get("apps", []) or [])
        for app in apps:
            if isinstance(app, dict):
                out.append(parse_app_entry(app, node_id, idx))
                idx += 1

    for app in list(config.get("apps", []) or []):
        if isinstance(app, dict):
            out.append(parse_app_entry(app, "", idx))
            idx += 1

    return out


def validate_app_specs(apps: list[AppSpec], topo: ClabTopology) -> None:
    names_seen: set[str] = set()
    sink_ports: dict[tuple[str, str, str, int], str] = {}

    for spec in apps:
        if spec.name in names_seen:
            raise ValueError(f"duplicate app name: {spec.name}")
        names_seen.add(spec.name)

        if spec.node not in topo.node_names:
            raise ValueError(f"app '{spec.name}' node not found in topology: {spec.node}")

        target = infer_sender_target(spec)
        if target in {"127.0.0.1", "localhost", "::1"}:
            raise ValueError(
                f"app '{spec.name}' sender target must be remote node ip, got {target}"
            )

        sink_socket = infer_sink_socket(spec)
        if sink_socket is not None:
            key = (spec.node, sink_socket[0], sink_socket[1], sink_socket[2])
            conflict = sink_ports.get(key, "")
            if conflict:
                raise ValueError(
                    "sink port conflict on node "
                    f"{spec.node}: app '{spec.name}' conflicts with '{conflict}' "
                    f"for {sink_socket[0]} {sink_socket[1]}:{sink_socket[2]}"
                )
            sink_ports[key] = spec.name


def parse_background_pid(output: str) -> int:
    for token in output.split():
        if token.isdigit():
            return int(token)
    return 0


def load_yaml_mapping(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    return dict(data) if isinstance(data, dict) else {}


def build_default_routerd_spec(node: str, log_level: str) -> dict[str, Any]:
    return {
        "name": "routingd",
        "kind": "routingd",
        "bin": "/irp/bin/routingd",
        "args": [
            "--config",
            f"/irp/configs/{node}.yaml",
            "--log-level",
            str(log_level),
        ],
        "env": {},
        "restart": "always",
        "max_restarts": -1,
        "log_file": "/tmp/routerd.log",
    }


def to_supervisor_app(spec: AppSpec, *, enabled: bool) -> dict[str, Any]:
    return {
        "name": spec.name,
        "kind": spec.kind,
        "bin": spec.bin,
        "args": list(spec.args),
        "env": dict(spec.env),
        "enabled": bool(enabled),
        "restart": spec.restart,
        "max_restarts": int(spec.max_restarts),
        "start_delay_s": float(spec.delay_s),
        "log_file": spec.log_file,
    }


def configure_node_supervisor_apps(
    *,
    use_sudo: bool,
    lab_name: str,
    config_dir: Path,
    topo: ClabTopology,
    app_specs: list[AppSpec],
    log_level: str,
    enabled_overrides: dict[str, bool] | None = None,
    only_nodes: set[str] | None = None,
    event_time_s: float = 0.0,
    event_name: str = "node_supervisor_restarted",
) -> list[dict[str, Any]]:
    grouped: dict[str, list[AppSpec]] = {node: [] for node in topo.node_names}
    for spec in app_specs:
        grouped.setdefault(spec.node, []).append(spec)

    out_dir = ensure_dir(config_dir / "_supervisor_unified")
    events: list[dict[str, Any]] = []

    for node in topo.node_names:
        if only_nodes is not None and node not in only_nodes:
            continue
        node_apps = grouped.get(node, [])
        base_cfg_path = config_dir / "_supervisor" / f"{node}.supervisor.yaml"
        base_cfg = load_yaml_mapping(base_cfg_path)

        routerd_cfg = base_cfg.get("routerd", {})
        if not isinstance(routerd_cfg, dict) or not routerd_cfg.get("bin"):
            routerd_cfg = build_default_routerd_spec(node=node, log_level=log_level)

        supervisor_cfg = {
            "node_id": str(base_cfg.get("node_id", node)),
            "tick_ms": int(base_cfg.get("tick_ms", 500)),
            "state_file": str(
                base_cfg.get("state_file", "/tmp/node_supervisor_state.json")
            ).strip()
            or "/tmp/node_supervisor_state.json",
            "routerd": routerd_cfg,
            "apps": [
                to_supervisor_app(
                    item,
                    enabled=(
                        enabled_overrides.get(item.name, True)
                        if enabled_overrides is not None
                        else True
                    ),
                )
                for item in node_apps
            ],
        }

        local_cfg_file = out_dir / f"{node}.supervisor.yaml"
        local_cfg_file.write_text(
            yaml.safe_dump(supervisor_cfg, sort_keys=False),
            encoding="utf-8",
        )

        container = container_name(lab_name, node)
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "cp",
                    str(local_cfg_file.resolve()),
                    f"{container}:/irp/configs/{node}.supervisor.yaml",
                ],
                use_sudo,
            ),
            check=True,
            capture_output=True,
        )

        restart_cmd = (
            "if [ -f /tmp/node_supervisor.pid ]; then "
            "PID=$(cat /tmp/node_supervisor.pid 2>/dev/null || true); "
            "if [ -n \"$PID\" ]; then kill \"$PID\" >/dev/null 2>&1 || true; fi; "
            "fi; "
            "pkill -x node_supervisor >/dev/null 2>&1 || true; "
            "SUPERVISOR_BIN=/irp/bin/node_supervisor; "
            "test -x \"$SUPERVISOR_BIN\" || "
            "{ echo 'missing /irp/bin/node_supervisor' >&2; exit 127; }; "
            "nohup \"$SUPERVISOR_BIN\" "
            f"--config /irp/configs/{node}.supervisor.yaml "
            ">/tmp/node_supervisor.log 2>&1 & echo $! >/tmp/node_supervisor.pid; "
            "cat /tmp/node_supervisor.pid"
        )
        restart_proc = run_cmd(
            with_sudo(
                ["docker", "exec", container, "sh", "-lc", restart_cmd],
                use_sudo,
            ),
            check=False,
            capture_output=True,
        )
        if restart_proc.returncode != 0:
            out_text = (restart_proc.stdout or "").strip()
            raise RuntimeError(
                f"failed to restart node_supervisor on {node}: {out_text}"
            )
        pid = parse_background_pid(restart_proc.stdout or "")

        events.append(
            {
                "t_s": round(event_time_s, 3),
                "event": event_name,
                "node": node,
                "pid": pid,
                "apps": [item.name for item in node_apps],
            }
        )

    return events


def apps_from_supervisor_obj(node: str, supervisor_obj: dict[str, Any]) -> list[dict[str, Any]]:
    if not isinstance(supervisor_obj, dict):
        return []
    processes = list(supervisor_obj.get("processes", []))
    out: list[dict[str, Any]] = []
    for item in processes:
        if not isinstance(item, dict):
            continue
        name = str(item.get("name", "")).strip()
        kind = str(item.get("kind", "")).strip().lower()
        if name == "routingd" or kind == "routingd":
            continue
        rec = dict(item)
        rec["node"] = node
        out.append(rec)
    return sorted(out, key=lambda x: str(x.get("name", "")))


def main() -> int:
    args = parse_args()
    cfg_path = Path(str(args.config)).expanduser().resolve()
    if not cfg_path.exists():
        raise FileNotFoundError(f"config file not found: {cfg_path}")

    with cfg_path.open("r", encoding="utf-8") as f:
        config = yaml.safe_load(f) or {}
    if not isinstance(config, dict):
        raise ValueError("config must be a mapping")

    topology_key, topology_value = resolve_topology_spec(str(config.get("topology", "")))
    protocol = str(config.get("protocol", "ospf")).strip().lower()
    if protocol not in {"ospf", "rip", "irp"}:
        raise ValueError("protocol must be one of: ospf, rip, irp")

    routing = dict(config.get("routing", {}) or {})
    routing_alpha = float(routing.get("alpha", 1.0))
    routing_beta = float(routing.get("beta", 2.0))

    duration_s = max(1.0, float(config.get("duration_s", 60.0)))
    poll_interval_s = max(0.2, float(args.poll_interval_s))

    runlab_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "run_routerd_lab.py"),
        "--protocol",
        protocol,
        "--keep-lab",
        "--check-min-routes",
        "0",
        "--check-max-wait-s",
        str(float(config.get("precheck_max_wait_s", 20.0))),
        "--check-poll-interval-s",
        "1",
        "--check-tail-lines",
        str(int(config.get("precheck_tail_lines", 120))),
        "--sudo" if bool(args.sudo) else "--no-sudo",
    ]
    if topology_key == "profile":
        runlab_cmd.extend(["--profile", topology_value])
    else:
        runlab_cmd.extend(["--topology-file", topology_value])
    if protocol == "irp":
        runlab_cmd.extend([
            "--routing-alpha",
            str(routing_alpha),
            "--routing-beta",
            str(routing_beta),
        ])

    print("[1/3] deploy + bootstrap lab")
    print("cmd:", " ".join(shlex.quote(x) for x in runlab_cmd))
    runlab_proc = run_cmd(runlab_cmd, check=False)
    runlab_output = runlab_proc.stdout or ""
    if runlab_output.strip():
        print(runlab_output.strip())

    parsed = parse_keyed_output(runlab_output)
    missing = [k for k in KNOWN_KEYS if not parsed.get(k)]
    if missing:
        raise RuntimeError(
            "failed to parse run_routerd_lab output, missing keys: " + ", ".join(missing)
        )

    topology_file = Path(parsed["topology_file"]).expanduser().resolve()
    config_dir = Path(parsed["configs_dir"]).expanduser().resolve()
    deploy_env_file = Path(parsed["deploy_env_file"]).expanduser().resolve()
    lab_name = parsed["lab_name"]
    deploy_env = load_env_file(deploy_env_file)
    deploy_env["CLAB_LAB_NAME"] = lab_name

    if runlab_proc.returncode != 0:
        raise RuntimeError(
            f"run_routerd_lab failed ({runlab_proc.returncode}) before experiment start"
        )

    topo = load_clab_topology(topology_file)
    node_ports: dict[str, int] = {
        node: load_management_http_port(config_dir, node) for node in topo.node_names
    }

    explicit_apps = collect_explicit_apps(config)
    apps_mode = "explicit_apps"
    traffic_summary: dict[str, Any] = {
        "enabled": False,
        "legacy_mode": False,
    }
    if explicit_apps:
        app_specs = explicit_apps
    else:
        apps_mode = "legacy_traffic"
        app_specs, traffic_summary = build_legacy_traffic_apps(
            traffic_cfg=dict(config.get("traffic", {}) or {}),
            topo=topo,
            duration_s=duration_s,
        )

    validate_app_specs(app_specs, topo)

    app_events: list[dict[str, Any]] = []
    app_enabled_overrides: dict[str, bool] = {
        spec.name: True for spec in app_specs
    }
    supervisor_log_level = str(deploy_env.get("ROMAM_LOG_LEVEL", "INFO"))

    print("[2/3] update node_supervisor app specs")
    if app_specs:
        app_events.extend(
            configure_node_supervisor_apps(
                use_sudo=bool(args.sudo),
                lab_name=lab_name,
                config_dir=config_dir,
                topo=topo,
                app_specs=app_specs,
                log_level=supervisor_log_level,
                enabled_overrides=app_enabled_overrides,
            )
        )
    else:
        print("no app configured (apps/node_apps empty and legacy traffic disabled)")

    faults = load_faults(config.get("faults", []))
    pending_faults = list(faults)
    applied_faults: list[dict[str, Any]] = []

    min_routes_required = max(0, len(topo.node_names) - 1)
    samples: list[dict[str, Any]] = []
    converged_at_s: float | None = None

    print("[3/3] poll /v1/routes + /v1/metrics + node_supervisor state")
    start_t = time.monotonic()
    end_t = start_t + duration_s
    next_poll = start_t

    final_app_state: dict[str, list[dict[str, Any]]] = {
        node: [] for node in topo.node_names
    }

    while True:
        now = time.monotonic()
        if now < next_poll:
            time.sleep(min(0.2, next_poll - now))
            continue

        elapsed = now - start_t
        while pending_faults and elapsed >= pending_faults[0].time_s:
            fault = pending_faults.pop(0)
            fault_events: list[dict[str, Any]] = []
            if fault.type == "link_down":
                applied = apply_link_down_fault(
                    use_sudo=bool(args.sudo),
                    lab_name=lab_name,
                    topo=topo,
                    fault=fault,
                )
            elif fault.type in {"app_stop", "app_start"}:
                applied, fault_events = apply_app_toggle_fault(
                    use_sudo=bool(args.sudo),
                    lab_name=lab_name,
                    config_dir=config_dir,
                    topo=topo,
                    app_specs=app_specs,
                    app_enabled_overrides=app_enabled_overrides,
                    log_level=supervisor_log_level,
                    fault=fault,
                    event_time_s=elapsed,
                )
                app_events.extend(fault_events)
            else:
                applied = {
                    "time_s": fault.time_s,
                    "type": fault.type,
                    "ok": False,
                    "error": "unsupported fault type",
                    "data": fault.data,
                }
            applied["applied_at_s"] = elapsed
            applied_faults.append(applied)
            app_events.append(
                {
                    "t_s": round(elapsed, 3),
                    "event": "fault_applied",
                    "fault": applied,
                }
            )

        node_data: dict[str, Any] = {}
        route_counts: list[int] = []
        sample_errors: list[str] = []
        sample_apps_by_node: dict[str, list[dict[str, Any]]] = {}

        for node in topo.node_names:
            port = node_ports.get(node, 0)
            container = container_name(lab_name, node)
            routes_obj = fetch_mgmt_json(
                use_sudo=bool(args.sudo),
                container=container,
                port=port,
                path="/v1/routes",
            )
            metrics_obj = fetch_mgmt_json(
                use_sudo=bool(args.sudo),
                container=container,
                port=port,
                path="/v1/metrics",
            )
            supervisor_obj = fetch_supervisor_state(
                use_sudo=bool(args.sudo),
                container=container,
            )
            routes = list(routes_obj.get("routes", [])) if isinstance(routes_obj, dict) else []
            route_count = (
                int(metrics_obj.get("route_count", len(routes)))
                if isinstance(metrics_obj, dict)
                else len(routes)
            )
            route_counts.append(route_count)
            if not routes_obj:
                sample_errors.append(f"{node}: routes api empty")
            if not metrics_obj:
                sample_errors.append(f"{node}: metrics api empty")
            if not supervisor_obj:
                sample_errors.append(f"{node}: node_supervisor state empty")

            node_data[node] = {
                "management_http_port": int(port),
                "route_count": route_count,
                "routes": routes,
                "metrics": metrics_obj,
                "node_supervisor": supervisor_obj,
            }
            sample_apps_by_node[node] = apps_from_supervisor_obj(node, supervisor_obj)

        all_converged = all(count >= min_routes_required for count in route_counts)
        if all_converged and converged_at_s is None:
            converged_at_s = elapsed
        final_app_state = sample_apps_by_node

        samples.append(
            {
                "t_s": round(elapsed, 3),
                "all_converged": all_converged,
                "min_routes_required": min_routes_required,
                "node_data": node_data,
                "apps": sample_apps_by_node,
                "errors": sample_errors,
            }
        )

        if now >= end_t:
            break
        next_poll += poll_interval_s

    fault_reconvergence: list[dict[str, Any]] = []
    for fault in applied_faults:
        t0 = float(fault.get("applied_at_s", 0.0))
        reconverged_at = None
        for sample in samples:
            if float(sample.get("t_s", 0.0)) < t0:
                continue
            if bool(sample.get("all_converged", False)):
                reconverged_at = float(sample.get("t_s", 0.0))
                break
        fault_reconvergence.append(
            {
                "fault": fault,
                "reconverged_at_s": reconverged_at,
                "reconvergence_delay_s": None
                if reconverged_at is None
                else max(0.0, reconverged_at - t0),
            }
        )

    report: dict[str, Any] = {
        "config_file": str(cfg_path),
        "lab_name": lab_name,
        "protocol": protocol,
        "topology_file": str(topology_file),
        "config_dir": str(config_dir),
        "duration_s": duration_s,
        "poll_interval_s": poll_interval_s,
        "min_routes_required": min_routes_required,
        "traffic": traffic_summary,
        "apps": {
            "mode": apps_mode,
            "count": len(app_specs),
            "specs": [asdict(spec) for spec in app_specs],
            "events": app_events,
            "managed_by": "node_supervisor",
            "final_state": final_app_state,
        },
        "routing": {
            "alpha": routing_alpha,
            "beta": routing_beta,
        },
        "convergence": {
            "initial_converged_at_s": converged_at_s,
            "fault_reconvergence": fault_reconvergence,
        },
        "faults_applied": applied_faults,
        "samples": samples,
    }

    output_json = str(args.output_json).strip()
    if not output_json:
        out_dir = ensure_dir(REPO_ROOT / "results" / "runs" / "unified_experiments" / lab_name)
        output_json = str(out_dir / f"report_{now_tag().lower()}.json")

    dump_json(Path(output_json), report)
    print(f"report_json: {output_json}")
    print(f"initial_converged_at_s: {converged_at_s}")

    if not bool(args.keep_lab):
        run_containerlab_destroy(
            topology_file=topology_file,
            lab_name=lab_name,
            deploy_env=deploy_env,
            use_sudo=bool(args.sudo),
        )
        print("lab destroyed")
    else:
        print("keep_lab enabled: skip destroy")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
