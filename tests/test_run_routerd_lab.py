from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

import pytest


def _load_module():
    module_path = Path("tools/run_routerd_lab.py").resolve()
    spec = importlib.util.spec_from_file_location("run_routerd_lab", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load tools/run_routerd_lab.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_infer_protocol_default_and_explicit() -> None:
    module = _load_module()
    assert module.infer_protocol([]) == "ospf"
    assert module.infer_protocol(["--protocol", "rip"]) == "rip"
    assert module.infer_protocol(["--x", "1", "--protocol=irp"]) == "irp"


def test_parse_generator_output_missing_key_raises() -> None:
    module = _load_module()
    with pytest.raises(RuntimeError):
        module.parse_generator_output("topology_file: /tmp/a.clab.yaml\n")


def test_run_generate_routerd_lab_returns_context(tmp_path: Path, monkeypatch) -> None:
    module = _load_module()

    topo = tmp_path / "demo.clab.yaml"
    cfg = tmp_path / "cfg"
    env_file = tmp_path / "demo.env"
    topo.write_text("name: demo\n", encoding="utf-8")
    cfg.mkdir(parents=True, exist_ok=True)
    env_file.write_text("ROMAM_LOG_LEVEL=DEBUG\n", encoding="utf-8")

    output = (
        f"topology_file: {topo}\n"
        f"configs_dir: {cfg}\n"
        f"deploy_env_file: {env_file}\n"
        "lab_name: demo-lab\n"
    )

    seen_cmd: list[str] = []

    def fake_run_cmd(  # type: ignore[no-untyped-def]
        cmd,
        check=True,
        capture_output=True,
        env=None,
        cwd=None,
    ):
        del check, capture_output, env, cwd
        seen_cmd.extend(cmd)
        return output

    monkeypatch.setattr(module, "run_cmd", fake_run_cmd)
    ctx = module.run_generate_routerd_lab(
        gen_args=["--profile", "line3"],
        use_sudo=True,
    )
    assert str(module.REPO_ROOT / "tools" / "generate_routerd_lab.py") in " ".join(seen_cmd)
    assert "--sudo" in seen_cmd
    assert ctx.topology_file == topo.resolve()
    assert ctx.configs_dir == cfg.resolve()
    assert ctx.deploy_env_file == env_file.resolve()
    assert ctx.lab_name == "demo-lab"
    assert ctx.deploy_env["ROMAM_LOG_LEVEL"] == "DEBUG"
    assert ctx.deploy_env["CLAB_LAB_NAME"] == "demo-lab"
