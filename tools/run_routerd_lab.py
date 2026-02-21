#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))
SRC_DIR = REPO_ROOT / "src"
GO_TRAFFIC_SRC_DIR = REPO_ROOT / "src" / "applications_go"
GO_TRAFFIC_BIN_PATH = REPO_ROOT / "bin" / "traffic_app"
RUST_ROUTERD_CRATE_DIR = REPO_ROOT / "src" / "irp"
RUST_ROUTERD_BIN_PATH = REPO_ROOT / "bin" / "routingd"
RUST_NODE_SUPERVISOR_BIN_PATH = REPO_ROOT / "bin" / "node_supervisor"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

from tools.common import (
    parse_env_file,
    parse_keyed_output,
    run_clab_command,
)
from tools.common import (
    run_command as common_run_command,
)

GENERATOR_KEYS = ("topology_file", "configs_dir", "deploy_env_file", "lab_name")


@dataclass(frozen=True)
class GeneratedLabContext:
    topology_file: Path
    configs_dir: Path
    deploy_env_file: Path
    lab_name: str
    deploy_env: dict[str, str]


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description=(
            "Generate routerd lab configs, deploy with containerlab, "
            "bootstrap binaries in containers, run health checks, and optionally destroy the lab."
        )
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for containerlab/docker commands (default: enabled).",
    )
    parser.add_argument(
        "--keep-lab",
        action="store_true",
        help="Keep the lab running after checks (skip destroy).",
    )
    parser.add_argument(
        "--check-tail-lines",
        type=int,
        default=60,
        help="Number of tail lines read from /tmp/routerd.log per node during health checks.",
    )
    parser.add_argument(
        "--check-max-wait-s",
        type=float,
        default=10.0,
        help="Maximum wait time in seconds for convergence evidence during health checks.",
    )
    parser.add_argument(
        "--check-poll-interval-s",
        type=float,
        default=1.0,
        help="Health-check polling interval in seconds.",
    )
    parser.add_argument(
        "--check-min-routes",
        type=int,
        default=-1,
        help="Minimum route count expected during health checks (-1 means n_nodes-1).",
    )
    parser.add_argument(
        "--check-output-json",
        default="",
        help="Path to health-check JSON report (optional).",
    )
    args, gen_args = parser.parse_known_args()
    return args, gen_args


def with_sudo(cmd: list[str], use_sudo: bool) -> list[str]:
    return ["sudo", *cmd] if use_sudo else cmd


def run_cmd(
    cmd: list[str],
    check: bool = True,
    capture_output: bool = True,
    env: dict[str, str] | None = None,
    cwd: Path | None = None,
) -> str:
    result = common_run_command(
        cmd,
        check=check,
        capture_output=capture_output,
        env=env,
        cwd=cwd,
    )
    return (result.stdout or "").strip()


def infer_protocol(gen_args: list[str]) -> str:
    for idx, arg in enumerate(gen_args):
        if arg == "--protocol" and idx + 1 < len(gen_args):
            return str(gen_args[idx + 1]).strip().lower()
        if arg.startswith("--protocol="):
            return arg.split("=", maxsplit=1)[1].strip().lower()
    return "ospf"


def parse_generator_output(text: str) -> tuple[Path, Path, Path, str]:
    parsed = parse_keyed_output(text, keys=GENERATOR_KEYS)
    missing = [key for key in GENERATOR_KEYS if not parsed.get(key)]
    if missing:
        raise RuntimeError(
            "Cannot parse generate_routerd_lab.py output; missing: "
            + ", ".join(missing)
        )
    topology_path = Path(parsed["topology_file"]).expanduser().resolve()
    configs_dir = Path(parsed["configs_dir"]).expanduser().resolve()
    deploy_env_file = Path(parsed["deploy_env_file"]).expanduser().resolve()
    lab_name = parsed["lab_name"]
    if not lab_name:
        raise RuntimeError("Cannot parse lab_name from generate_routerd_lab.py output")
    return topology_path, configs_dir, deploy_env_file, lab_name


def run_generate_routerd_lab(
    *,
    gen_args: list[str],
    use_sudo: bool,
) -> GeneratedLabContext:
    gen_cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "generate_routerd_lab.py"),
        *gen_args,
    ]
    if use_sudo:
        gen_cmd.append("--sudo")

    print(f"[1/7] Generate lab configs: {' '.join(gen_cmd)}")
    generated_text = run_cmd(gen_cmd, check=True, capture_output=True)
    print(generated_text)

    topology_file, configs_dir, deploy_env_file, lab_name = parse_generator_output(generated_text)
    deploy_env = parse_env_file(deploy_env_file)
    deploy_env["CLAB_LAB_NAME"] = lab_name
    return GeneratedLabContext(
        topology_file=topology_file,
        configs_dir=configs_dir,
        deploy_env_file=deploy_env_file,
        lab_name=lab_name,
        deploy_env=deploy_env,
    )


def resolve_containerlab_bin() -> str:
    clab_bin = shutil.which("containerlab")
    if clab_bin:
        return clab_bin
    raise RuntimeError(
        "containerlab not found in PATH; install it or add it to PATH (e.g. ~/.local/bin)."
    )


def list_node_names(topology_file: Path) -> list[str]:
    with topology_file.open("r", encoding="utf-8") as f:
        topo = yaml.safe_load(f) or {}
    nodes = dict(topo.get("topology", {}).get("nodes", {}))
    if not nodes:
        raise RuntimeError(f"No nodes found in topology file: {topology_file}")
    return [str(name) for name in nodes.keys()]


def clab_container_name(lab_name: str, node_name: str) -> str:
    return f"clab-{lab_name}-{node_name}"


def container_has_executable(use_sudo: bool, container: str, path: str) -> bool:
    return (
        subprocess.run(
            with_sudo(["docker", "exec", container, "sh", "-lc", f"test -x {path}"], use_sudo),
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ).returncode
        == 0
    )


def run_containerlab(
    cmd: list[str],
    use_sudo: bool,
    env_overrides: dict[str, str],
    check: bool,
) -> None:
    proc = run_clab_command(
        cmd,
        use_sudo=use_sudo,
        env_overrides=env_overrides,
        check=check,
        capture_output=True,
    )
    out = (proc.stdout or "").strip()
    if out:
        print(out)


def build_traffic_app_binary() -> Path | None:
    if not GO_TRAFFIC_SRC_DIR.exists():
        raise RuntimeError(f"go traffic source dir not found: {GO_TRAFFIC_SRC_DIR}")

    go_bin = shutil.which("go")
    if go_bin is None:
        if GO_TRAFFIC_BIN_PATH.exists():
            print("[warn] `go` not found; using existing prebuilt traffic_app binary")
            return GO_TRAFFIC_BIN_PATH
        raise RuntimeError(
            "`go` not found in PATH and no prebuilt binary at bin/traffic_app; "
            "install Go or run `make build-traffic-app-go` first"
        )

    GO_TRAFFIC_BIN_PATH.parent.mkdir(parents=True, exist_ok=True)
    go_os = os.environ.get("ROMAM_TRAFFIC_GOOS", "linux").strip() or "linux"
    go_arch = os.environ.get("ROMAM_TRAFFIC_GOARCH", "amd64").strip() or "amd64"
    cgo_enabled = os.environ.get("ROMAM_TRAFFIC_CGO_ENABLED", "0").strip() or "0"
    build_cmd = [go_bin, "build", "-o", str(GO_TRAFFIC_BIN_PATH), "./cmd/traffic_app"]
    build_env = {
        **os.environ,
        "GOOS": go_os,
        "GOARCH": go_arch,
        "CGO_ENABLED": cgo_enabled,
    }
    build_proc = common_run_command(
        build_cmd,
        check=False,
        capture_output=True,
        env=build_env,
        cwd=GO_TRAFFIC_SRC_DIR,
    )
    output = (build_proc.stdout or "").strip()
    if build_proc.returncode != 0:
        if GO_TRAFFIC_BIN_PATH.exists():
            print("[warn] go traffic app build failed; using existing prebuilt binary")
            return GO_TRAFFIC_BIN_PATH
        raise RuntimeError(
            "go traffic app build failed.\n"
            f"{output}\n"
            "fix build errors or run `make build-traffic-app-go` first"
        )

    if output:
        print(output)
    GO_TRAFFIC_BIN_PATH.chmod(0o755)
    print(
        f"go traffic build target: GOOS={go_os} GOARCH={go_arch} CGO_ENABLED={cgo_enabled}"
    )
    return GO_TRAFFIC_BIN_PATH


def build_rust_router_binaries() -> tuple[Path, Path]:
    if not RUST_ROUTERD_CRATE_DIR.exists():
        raise RuntimeError(f"rust routerd crate dir not found: {RUST_ROUTERD_CRATE_DIR}")

    cargo_bin = shutil.which("cargo")
    if cargo_bin is None:
        if RUST_ROUTERD_BIN_PATH.exists() and RUST_NODE_SUPERVISOR_BIN_PATH.exists():
            print("[warn] `cargo` not found; using existing prebuilt rust binaries")
            return RUST_ROUTERD_BIN_PATH, RUST_NODE_SUPERVISOR_BIN_PATH
        raise RuntimeError(
            "`cargo` not found in PATH and no prebuilt binaries at bin/routingd + "
            "bin/node_supervisor; "
            "install Rust toolchain or run `make build-routerd-rs` first"
        )

    target_profile = os.environ.get("ROMAM_ROUTERD_RS_PROFILE", "release").strip() or "release"
    target_triple = (
        os.environ.get("ROMAM_ROUTERD_RS_TARGET", "x86_64-unknown-linux-musl").strip()
        or "x86_64-unknown-linux-musl"
    )
    build_cmd = [cargo_bin, "build", f"--{target_profile}", "--target", target_triple]
    build_proc = common_run_command(
        build_cmd,
        check=False,
        capture_output=True,
        cwd=RUST_ROUTERD_CRATE_DIR,
    )
    output = (build_proc.stdout or "").strip()
    if build_proc.returncode != 0:
        target_hint = ""
        missing_target = "target may not be installed" in output.lower()
        missing_std = "can't find crate for `std`" in output.lower()
        if missing_target or missing_std:
            target_hint = (
                f"\nInstall target first: rustup target add {target_triple}"
            )
        raise RuntimeError(
            "rust build failed.\n"
            f"{output}\n"
            f"{target_hint}\n"
            "fix build errors or run `make build-routerd-rs` first"
        )

    if output:
        print(output)

    routingd_candidates = [
        RUST_ROUTERD_CRATE_DIR / "target" / target_triple / target_profile / "routingd",
        RUST_ROUTERD_CRATE_DIR / "target" / target_triple / target_profile / "irp_routerd_rs",
        RUST_ROUTERD_CRATE_DIR / "target" / target_profile / "routingd",
        RUST_ROUTERD_CRATE_DIR / "target" / target_profile / "irp_routerd_rs",
    ]
    routingd_built = next((path for path in routingd_candidates if path.exists()), None)
    if routingd_built is None:
        raise RuntimeError(
            "expected rust routerd binary missing after build: "
            + ", ".join(str(path) for path in routingd_candidates)
        )

    supervisor_candidates = [
        RUST_ROUTERD_CRATE_DIR / "target" / target_triple / target_profile / "node_supervisor",
        RUST_ROUTERD_CRATE_DIR / "target" / target_profile / "node_supervisor",
    ]
    supervisor_built = next((path for path in supervisor_candidates if path.exists()), None)
    if supervisor_built is None:
        raise RuntimeError(
            "expected node_supervisor binary missing after build: "
            + ", ".join(str(path) for path in supervisor_candidates)
        )

    RUST_ROUTERD_BIN_PATH.parent.mkdir(parents=True, exist_ok=True)
    RUST_NODE_SUPERVISOR_BIN_PATH.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(routingd_built, RUST_ROUTERD_BIN_PATH)
    shutil.copy2(supervisor_built, RUST_NODE_SUPERVISOR_BIN_PATH)
    RUST_ROUTERD_BIN_PATH.chmod(0o755)
    RUST_NODE_SUPERVISOR_BIN_PATH.chmod(0o755)
    print(f"rust routerd build profile: {target_profile} target: {target_triple}")
    return RUST_ROUTERD_BIN_PATH, RUST_NODE_SUPERVISOR_BIN_PATH


def bootstrap_routerd(
    topology_file: Path,
    configs_dir: Path,
    lab_name: str,
    log_level: str,
    use_sudo: bool,
    routerd_rs_bin: Path,
    node_supervisor_bin: Path,
    traffic_app_bin: Path | None,
) -> None:
    supervisor_dir = configs_dir / "_supervisor"
    supervisor_dir.mkdir(parents=True, exist_ok=True)

    node_names = list_node_names(topology_file)
    for node_name in node_names:
        container = clab_container_name(lab_name, node_name)
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "exec",
                    container,
                    "mkdir",
                    "-p",
                    "/irp/configs",
                    "/irp/bin",
                ],
                use_sudo,
            ),
            check=True,
            capture_output=False,
        )
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "cp",
                    str((configs_dir / f"{node_name}.yaml").resolve()),
                    f"{container}:/irp/configs/{node_name}.yaml",
                ],
                use_sudo,
            ),
            check=True,
            capture_output=False,
        )
        if routerd_rs_bin.exists():
            run_cmd(
                with_sudo(
                    [
                        "docker",
                        "cp",
                        str(routerd_rs_bin.resolve()),
                        f"{container}:/irp/bin/routingd",
                    ],
                    use_sudo,
                ),
                check=True,
                capture_output=False,
            )
            run_cmd(
                with_sudo(
                    [
                        "docker",
                        "exec",
                        container,
                        "sh",
                        "-lc",
                        (
                            "chmod +x /irp/bin/routingd "
                            "&& ln -sf /irp/bin/routingd /irp/bin/irp_routerd_rs"
                        ),
                    ],
                    use_sudo,
                ),
                check=True,
                capture_output=False,
            )
        else:
            has_routerd = subprocess.run(
                with_sudo(
                    [
                        "docker",
                        "exec",
                        container,
                        "sh",
                        "-lc",
                        "test -x /irp/bin/routingd || test -x /irp/bin/irp_routerd_rs",
                    ],
                    use_sudo,
                ),
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            ).returncode == 0
            if not has_routerd:
                raise RuntimeError(
                    "node image does not include /irp/bin/routingd and local binary is missing: "
                    f"{routerd_rs_bin}. Run `make build-routerd-rs` first."
                )

        if node_supervisor_bin.exists():
            run_cmd(
                with_sudo(
                    [
                        "docker",
                        "cp",
                        str(node_supervisor_bin.resolve()),
                        f"{container}:/irp/bin/node_supervisor",
                    ],
                    use_sudo,
                ),
                check=True,
                capture_output=False,
            )
            run_cmd(
                with_sudo(
                    ["docker", "exec", container, "chmod", "+x", "/irp/bin/node_supervisor"],
                    use_sudo,
                ),
                check=True,
                capture_output=False,
            )
        else:
            has_supervisor = subprocess.run(
                with_sudo(
                    ["docker", "exec", container, "sh", "-lc", "test -x /irp/bin/node_supervisor"],
                    use_sudo,
                ),
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            ).returncode == 0
            if not has_supervisor:
                raise RuntimeError(
                    "node image does not include /irp/bin/node_supervisor and local binary is "
                    f"missing: {node_supervisor_bin}. Run `make build-routerd-rs` first."
                )
        if traffic_app_bin is not None and subprocess.run(
            with_sudo(
                ["docker", "exec", container, "sh", "-lc", "test -x /irp/bin/traffic_app"],
                use_sudo,
            ),
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ).returncode != 0:
            if not traffic_app_bin.exists():
                raise RuntimeError(
                    "node image does not include /irp/bin/traffic_app and local binary is missing: "
                    f"{traffic_app_bin}. Run `make build-traffic-app-go` first."
                )
            run_cmd(
                with_sudo(
                    [
                        "docker",
                        "cp",
                        str(traffic_app_bin.resolve()),
                        f"{container}:/irp/bin/traffic_app",
                    ],
                    use_sudo,
                ),
                check=True,
                capture_output=False,
            )
            run_cmd(
                with_sudo(
                    ["docker", "exec", container, "chmod", "+x", "/irp/bin/traffic_app"],
                    use_sudo,
                ),
                check=True,
                capture_output=False,
            )

        supervisor_cfg = {
            "node_id": str(node_name),
            "tick_ms": 500,
            "state_file": "/tmp/node_supervisor_state.json",
            "routerd": {
                "name": "routingd",
                "kind": "routingd",
                "bin": "/irp/bin/routingd",
                "args": [
                    "--config",
                    f"/irp/configs/{node_name}.yaml",
                    "--log-level",
                    str(log_level),
                ],
                "env": {},
                "restart": "always",
                "max_restarts": -1,
                "log_file": "/tmp/routerd.log",
            },
            "apps": [],
        }
        supervisor_cfg_file = supervisor_dir / f"{node_name}.supervisor.yaml"
        supervisor_cfg_file.write_text(
            yaml.safe_dump(supervisor_cfg, sort_keys=False),
            encoding="utf-8",
        )
        run_cmd(
            with_sudo(
                [
                    "docker",
                    "cp",
                    str(supervisor_cfg_file.resolve()),
                    f"{container}:/irp/configs/{node_name}.supervisor.yaml",
                ],
                use_sudo,
            ),
            check=True,
            capture_output=False,
        )

        daemon_cmd = (
            "if [ -f /tmp/node_supervisor.pid ]; then "
            "PID=$(cat /tmp/node_supervisor.pid 2>/dev/null || true); "
            "if [ -n \"$PID\" ]; then kill \"$PID\" >/dev/null 2>&1 || true; fi; "
            "fi; "
            "if [ -f /tmp/routerd.pid ]; then "
            "PID=$(cat /tmp/routerd.pid 2>/dev/null || true); "
            "if [ -n \"$PID\" ]; then kill \"$PID\" >/dev/null 2>&1 || true; fi; "
            "fi; "
            "pkill -x node_supervisor >/dev/null 2>&1 || true; "
            "pkill -x routingd >/dev/null 2>&1 || true; "
            "pkill -x irp_routerd_rs >/dev/null 2>&1 || true; "
            "SUPERVISOR_BIN=/irp/bin/node_supervisor; "
            "test -x \"$SUPERVISOR_BIN\" || "
            "{ echo 'missing /irp/bin/node_supervisor' >&2; exit 127; }; "
            "nohup \"$SUPERVISOR_BIN\" "
            f"--config /irp/configs/{node_name}.supervisor.yaml "
            ">/tmp/node_supervisor.log 2>&1 & echo $! >/tmp/node_supervisor.pid"
        )
        run_cmd(
            with_sudo(["docker", "exec", container, "sh", "-lc", daemon_cmd], use_sudo),
            check=True,
            capture_output=False,
        )


def main() -> int:
    args, gen_args = parse_args()
    protocol = infer_protocol(gen_args)

    generated = run_generate_routerd_lab(
        gen_args=gen_args,
        use_sudo=bool(args.sudo),
    )
    topology_file = generated.topology_file
    configs_dir = generated.configs_dir
    deploy_env_file = generated.deploy_env_file
    lab_name = generated.lab_name
    deploy_env = dict(generated.deploy_env)
    clab_bin = resolve_containerlab_bin()

    deploy_cmd = [
        clab_bin,
        "deploy",
        "-t",
        str(topology_file),
        "--name",
        lab_name,
        "--reconfigure",
    ]
    print(f"[2/7] Deploy lab: {' '.join(with_sudo(deploy_cmd, args.sudo))}")
    run_containerlab(deploy_cmd, use_sudo=bool(args.sudo), env_overrides=deploy_env, check=True)

    log_level = deploy_env.get("ROMAM_LOG_LEVEL", "INFO")
    node_names = list_node_names(topology_file)
    sample_container = clab_container_name(lab_name, node_names[0])
    sample_has_routerd = container_has_executable(
        bool(args.sudo), sample_container, "/irp/bin/routingd"
    ) or container_has_executable(bool(args.sudo), sample_container, "/irp/bin/irp_routerd_rs")
    sample_has_supervisor = container_has_executable(
        bool(args.sudo), sample_container, "/irp/bin/node_supervisor"
    )
    sample_has_traffic = container_has_executable(
        bool(args.sudo), sample_container, "/irp/bin/traffic_app"
    )

    if sample_has_routerd and sample_has_supervisor:
        print("[3/7] Detected routingd + node_supervisor in node image; skip local rust build")
        routerd_rs_bin = RUST_ROUTERD_BIN_PATH
        node_supervisor_bin = RUST_NODE_SUPERVISOR_BIN_PATH
    else:
        print("[3/7] Build rust routing binaries (routingd + node_supervisor)")
        routerd_rs_bin, node_supervisor_bin = build_rust_router_binaries()
        print(f"rust routingd ready: {routerd_rs_bin}")
        print(f"rust node_supervisor ready: {node_supervisor_bin}")

    if sample_has_traffic:
        print("[4/7] Detected traffic_app in node image; skip local go build")
        traffic_app_bin = None
    else:
        print("[4/7] Build traffic app data plane binary")
        traffic_app_bin = build_traffic_app_binary()
        print(f"go traffic app ready: {traffic_app_bin}")

    print("[5/7] Bootstrap node_supervisor + routingd inside each container")
    bootstrap_routerd(
        topology_file=topology_file,
        configs_dir=configs_dir,
        lab_name=lab_name,
        log_level=log_level,
        use_sudo=bool(args.sudo),
        routerd_rs_bin=routerd_rs_bin,
        node_supervisor_bin=node_supervisor_bin,
        traffic_app_bin=traffic_app_bin,
    )

    check_cmd = [
        sys.executable,
        str(REPO_ROOT / "tools" / "check_routerd_lab.py"),
        "--topology-file",
        str(topology_file),
        "--lab-name",
        lab_name,
        "--config-dir",
        str(configs_dir),
        "--expect-protocol",
        protocol,
        "--tail-lines",
        str(args.check_tail_lines),
        "--max-wait-s",
        str(args.check_max_wait_s),
        "--poll-interval-s",
        str(args.check_poll_interval_s),
        "--min-routes",
        str(args.check_min_routes),
    ]
    if args.check_output_json:
        check_cmd.extend(["--output-json", str(args.check_output_json)])
    if args.sudo:
        check_cmd.append("--sudo")

    try:
        print(f"[6/7] Check lab: {' '.join(check_cmd)}")
        run_cmd(check_cmd, check=True, capture_output=False)
    finally:
        if args.keep_lab:
            print("[7/7] Keep lab: skipping destroy (--keep-lab).")
            print(f"topology_file: {topology_file}")
            print(f"lab_name: {lab_name}")
            print(f"deploy_env_file: {deploy_env_file}")
        else:
            destroy_cmd = [
                clab_bin,
                "destroy",
                "-t",
                str(topology_file),
                "--name",
                lab_name,
                "--cleanup",
            ]
            print(f"[7/7] Destroy lab: {' '.join(with_sudo(destroy_cmd, args.sudo))}")
            run_containerlab(
                destroy_cmd,
                use_sudo=bool(args.sudo),
                env_overrides=deploy_env,
                check=False,
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
