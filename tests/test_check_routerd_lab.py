from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("exps/check_routerd_lab.py").resolve()
    spec = importlib.util.spec_from_file_location("check_routerd_lab", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load exps/check_routerd_lab.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_parse_last_route_count() -> None:
    module = _load_module()
    text = """
2026-02-11 INFO rpf.routerd: something
2026-02-11 INFO rpf.routerd: RIB/FIB updated: [(2, 2, 1.0), (3, 2, 2.0), (4, 4, 1.0)]
""".strip()
    assert module.parse_last_route_count(text) == 3


def test_parse_protocol_from_log() -> None:
    module = _load_module()
    text = """
2026-02-11 INFO rpf.routerd: routerd start: router_id=1 protocol=ospf bind=0.0.0.0:5500
""".strip()
    assert module.parse_protocol_from_log(text) == "ospf"
