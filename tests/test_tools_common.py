from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("tools/common.py").resolve()
    spec = importlib.util.spec_from_file_location("tools_common", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load tools/common.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_parse_keyed_output_extracts_known_lines() -> None:
    module = _load_module()
    text = (
        "topology_file: /tmp/a.clab.yaml\n"
        "lab_name: demo\n"
        "ignored: value\n"
    )
    out = module.parse_keyed_output(text, keys=("topology_file", "lab_name"))
    assert out["topology_file"] == "/tmp/a.clab.yaml"
    assert out["lab_name"] == "demo"
    assert "ignored" not in out


def test_parse_env_file_ignores_comments_and_invalid_lines(tmp_path: Path) -> None:
    module = _load_module()
    env = tmp_path / "x.env"
    env.write_text(
        "# comment\n"
        "A=1\n"
        "INVALID\n"
        "B = 2 \n",
        encoding="utf-8",
    )
    out = module.parse_env_file(env)
    assert out["A"] == "1"
    assert out["B"] == "2"
    assert "INVALID" not in out
