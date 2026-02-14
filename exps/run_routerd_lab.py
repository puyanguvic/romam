#!/usr/bin/env python3
# ruff: noqa: E402
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

import yaml

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = REPO_ROOT / "src"
GO_TRAFFIC_SRC_DIR = REPO_ROOT / "src" / "applications_go"
GO_TRAFFIC_BIN_PATH = REPO_ROOT / "bin" / "traffic_app"
if str(SRC_DIR) not in sys.path:
    sys.path.insert(0, str(SRC_DIR))

TOPOLOGY_LINE_RE = "topology_file:"
LAB_NAME_LINE_RE = "lab_name:"
CONFIGS_DIR_LINE_RE = "configs_dir:"
DEPLOY_ENV_LINE_RE = "deploy_env_file:"


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    parser = argparse.ArgumentParser(
        description=(
            "Generate routerd lab configs, deploy source topology with containerlab, "
            "bootstrap routerd in containers, run health check, and optionally destroy lab."
        )
    )
    parser.add_argument(
        "--sudo",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Use sudo for containerlab/docker operations (default: enabled).",
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
        help="Tail lines read from /tmp/routerd.log per node during health check.",
    )
    parser.add_argument(
        "--check-max-wait-s",
        type=float,
        default=10.0,
        help="Max wait seconds for convergence evidence during health check.",
    )
    parser.add_argument(
        "--check-poll-interval-s",
        type=float,
        default=1.0,
        help="Health check polling interval in seconds.",
    )
    parser.add_argument(
        "--check-min-routes",
        type=int,
        default=-1,
        help="Minimum route count expected during health check (-1 means n_nodes-1).",
    )
    parser.add_argument(
        "--check-output-json",
        default="",
        help="Optional path to write health-check JSON report.",
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
    result = subprocess.run(
        cmd,
        check=check,
        text=True,
        stdout=subprocess.PIPE if capture_output else None,
        stderr=subprocess.STDOUT if capture_output else None,
        env=env,
        cwd=str(cwd) if cwd is not None else None,
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
    topology_path: Path | None = None
    configs_dir: Path | None = None
    deploy_env_file: Path | None = None
    lab_name = ""

    for line in text.splitlines():
        stripped = line.strip()
        if stripped.startswith(TOPOLOGY_LINE_RE):
            topology_path = Path(stripped.split(":", maxsplit=1)[1].strip()).expanduser().resolve()
        elif stripped.startswith(CONFIGS_DIR_LINE_RE):
            configs_dir = Path(stripped.split(":", maxsplit=1)[1].strip()).expanduser().resolve()
        elif stripped.startswith(DEPLOY_ENV_LINE_RE):
            deploy_env_file = (
                Path(stripped.split(":", maxsplit=1)[1].strip()).expanduser().resolve()
            )
        elif stripped.startswith(LAB_NAME_LINE_RE):
            lab_name = stripped.split(":", maxsplit=1)[1].strip()

    if topology_path is None:
        raise RuntimeError("Cannot parse topology_file from generate_routerd_lab.py output")
    if configs_dir is None:
        raise RuntimeError("Cannot parse configs_dir from generate_routerd_lab.py output")
    if deploy_env_file is None:
        raise RuntimeError("Cannot parse deploy_env_file from generate_routerd_lab.py output")
    if not lab_name:
        raise RuntimeError("Cannot parse lab_name from generate_routerd_lab.py output")

    return topology_path, configs_dir, deploy_env_file, lab_name


def parse_env_file(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        text = line.strip()
        if not text or text.startswith("#"):
            continue
        if "=" not in text:
            continue
        key, value = text.split("=", maxsplit=1)
        out[key.strip()] = value.strip()
    return out


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


def run_containerlab(
    cmd: list[str],
    use_sudo: bool,
    env_overrides: dict[str, str],
    check: bool,
) -> None:
    if use_sudo:
        export_vars = [f"{key}={value}" for key, value in env_overrides.items()]
        run_cmd(
            ["sudo", "env", *export_vars, *cmd],
            check=check,
            capture_output=False,
        )
        return
    run_cmd(
        cmd,
        check=check,
        capture_output=False,
        env={**os.environ, **env_overrides},
    )


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
    try:
        output = run_cmd(
            build_cmd,
            check=True,
            capture_output=True,
            env=build_env,
            cwd=GO_TRAFFIC_SRC_DIR,
        )
    except subprocess.CalledProcessError as exc:
        if GO_TRAFFIC_BIN_PATH.exists():
            print("[warn] go traffic app build failed; using existing prebuilt binary")
            return GO_TRAFFIC_BIN_PATH
        build_output = (exc.stdout or "").strip()
        raise RuntimeError(
            "go traffic app build failed.\n"
            f"{build_output}\n"
            "fix build errors or run `make build-traffic-app-go` first"
        ) from exc

    if output:
        print(output)
    GO_TRAFFIC_BIN_PATH.chmod(0o755)
    print(
        f"go traffic build target: GOOS={go_os} GOARCH={go_arch} CGO_ENABLED={cgo_enabled}"
    )
    return GO_TRAFFIC_BIN_PATH


def bootstrap_routerd(
    topology_file: Path,
    configs_dir: Path,
    lab_name: str,
    log_level: str,
    use_sudo: bool,
    traffic_app_bin: Path | None,
) -> None:
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
                    "/irp/src",
                    "/irp/configs",
                    "/irp/bin",
                ],
                use_sudo,
            ),
            check=True,
            capture_output=False,
        )
        run_cmd(
            with_sudo(["docker", "cp", f"{SRC_DIR}/.", f"{container}:/irp/src"], use_sudo),
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
        if traffic_app_bin is not None:
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
        ensure_yaml_cmd = (
            "python3 -c 'import yaml' >/dev/null 2>&1 || "
            "(python3 -m ensurepip --upgrade >/dev/null 2>&1; "
            "python3 -m pip install --no-cache-dir pyyaml >/tmp/routerd_pip.log 2>&1)"
        )
        run_cmd(
            with_sudo(["docker", "exec", container, "sh", "-lc", ensure_yaml_cmd], use_sudo),
            check=True,
            capture_output=False,
        )
        daemon_cmd = (
            "PYTHONPATH=/irp/src nohup python3 -m irp.routerd "
            f"--config /irp/configs/{node_name}.yaml --log-level {log_level} "
            ">/tmp/routerd.log 2>&1 & echo $! >/tmp/routerd.pid"
        )
        run_cmd(
            with_sudo(["docker", "exec", container, "sh", "-lc", daemon_cmd], use_sudo),
            check=True,
            capture_output=False,
        )


def main() -> int:
    args, gen_args = parse_args()
    protocol = infer_protocol(gen_args)

    gen_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "generate_routerd_lab.py"),
        *gen_args,
    ]
    if args.sudo:
        gen_cmd.append("--sudo")

    print(f"[1/6] Generate lab configs: {' '.join(gen_cmd)}")
    generated_text = run_cmd(gen_cmd, check=True, capture_output=True)
    print(generated_text)

    topology_file, configs_dir, deploy_env_file, lab_name = parse_generator_output(generated_text)
    deploy_env = parse_env_file(deploy_env_file)
    deploy_env["CLAB_LAB_NAME"] = lab_name
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
    print(f"[2/6] Deploy lab: {' '.join(with_sudo(deploy_cmd, args.sudo))}")
    run_containerlab(deploy_cmd, use_sudo=bool(args.sudo), env_overrides=deploy_env, check=True)

    log_level = deploy_env.get("ROMAM_LOG_LEVEL", "INFO")
    print("[3/6] Build traffic app data plane binary")
    traffic_app_bin = build_traffic_app_binary()
    print(f"go traffic app ready: {traffic_app_bin}")

    print("[4/6] Bootstrap routerd inside each container")
    bootstrap_routerd(
        topology_file=topology_file,
        configs_dir=configs_dir,
        lab_name=lab_name,
        log_level=log_level,
        use_sudo=bool(args.sudo),
        traffic_app_bin=traffic_app_bin,
    )

    check_cmd = [
        sys.executable,
        str(REPO_ROOT / "exps" / "check_routerd_lab.py"),
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
        print(f"[5/6] Check lab: {' '.join(check_cmd)}")
        run_cmd(check_cmd, check=True, capture_output=False)
    finally:
        if args.keep_lab:
            print("[6/6] Keep lab: skipping destroy (--keep-lab).")
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
            print(f"[6/6] Destroy lab: {' '.join(with_sudo(destroy_cmd, args.sudo))}")
            run_containerlab(
                destroy_cmd,
                use_sudo=bool(args.sudo),
                env_overrides=deploy_env,
                check=False,
            )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
