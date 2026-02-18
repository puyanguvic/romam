from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("scripts/check_routerd_lab.py").resolve()
    spec = importlib.util.spec_from_file_location("check_routerd_lab", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load scripts/check_routerd_lab.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_parse_last_route_count() -> None:
    module = _load_module()
    text = (
        "2026-02-11T10:00:00Z  INFO irp::runtime::daemon: something\n"
        "2026-02-11T10:00:01Z  INFO irp::runtime::daemon: "
        "RIB/FIB updated: [(2, 2, 1.0), (3, 2, 2.0), (4, 4, 1.0)]"
    )
    assert module.parse_last_route_count(text) == 3


def test_parse_protocol_from_log() -> None:
    module = _load_module()
    text = (
        "2026-02-11T10:00:00Z  INFO irp::runtime::daemon: "
        "routerd start: router_id=1 protocol=ospf bind=0.0.0.0:5500"
    )
    assert module.parse_protocol_from_log(text) == "ospf"


def test_parse_config_bind_resolves_relative_to_topology_file(tmp_path: Path) -> None:
    module = _load_module()
    topology_file = tmp_path / "topologies" / "ring6-routerd.clab.yaml"
    topology_file.parent.mkdir(parents=True, exist_ok=True)
    topology_file.write_text("name: dummy\n", encoding="utf-8")

    node = {"binds": ["./routerd_configs/ring6:/irp/configs:ro"]}
    resolved = module.parse_config_bind(node, topology_file)
    assert resolved == (topology_file.parent / "routerd_configs" / "ring6").resolve()


def test_parse_config_bind_keeps_absolute_path() -> None:
    module = _load_module()
    topology_file = Path("/tmp/any/topology.clab.yaml")
    node = {"binds": ["/opt/configs:/irp/configs:ro"]}
    resolved = module.parse_config_bind(node, topology_file)
    assert resolved == Path("/opt/configs")


def test_check_daemon_running_matches_rust_process(monkeypatch) -> None:
    module = _load_module()
    outputs = iter(
        [
            "123\n",
            "/irp/bin/routingd --config /irp/configs/r1.yaml --log-level INFO\n",
        ]
    )

    def fake_run_cmd(_cmd, check=True):  # type: ignore[no-untyped-def]
        return next(outputs)

    monkeypatch.setattr(module, "run_cmd", fake_run_cmd)
    assert module._check_daemon_running(use_sudo=False, container="dummy") is True
