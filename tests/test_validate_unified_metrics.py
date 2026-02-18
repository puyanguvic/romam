from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("tools/validate_unified_metrics.py").resolve()
    spec = importlib.util.spec_from_file_location("validate_unified_metrics", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load tools/validate_unified_metrics.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_validate_scenario_payload_ok() -> None:
    module = _load_module()
    payload = {
        "config_file": "/tmp/x.yaml",
        "lab_name": "demo",
        "protocol": "ospf",
        "topology_file": "/tmp/t.clab.yaml",
        "duration_s": 10.0,
        "poll_interval_s": 1.0,
        "traffic": {},
        "apps": {"mode": "explicit_apps", "count": 0, "specs": []},
        "convergence": {"initial_converged_at_s": 1.2, "fault_reconvergence": []},
        "samples": [{"t_s": 0.0, "node_data": {}}],
    }
    mode, errors = module.validate_payload(payload, module.MODE_AUTO)
    assert mode == module.MODE_SCENARIO
    assert errors == []


def test_validate_benchmark_summary_payload_ok() -> None:
    module = _load_module()
    payload = {
        "experiment": "unified_convergence_benchmark",
        "mode": "convergence_benchmark",
        "protocol": "rip",
        "repeats": 1,
        "runs": [],
        "avg_probe_success_ratio": 1.0,
        "convergence_success_rate": 1.0,
    }
    mode, errors = module.validate_payload(payload, module.MODE_AUTO)
    assert mode == module.MODE_BENCHMARK_SUMMARY
    assert errors == []


def test_validate_payload_reports_missing_keys() -> None:
    module = _load_module()
    mode, errors = module.validate_payload({"protocol": "ospf"}, module.MODE_SCENARIO)
    assert mode == module.MODE_SCENARIO
    assert errors


def test_validate_scenario_accepts_ddr_protocol() -> None:
    module = _load_module()
    payload = {
        "config_file": "/tmp/x.yaml",
        "lab_name": "demo",
        "protocol": "ddr",
        "topology_file": "/tmp/t.clab.yaml",
        "duration_s": 10.0,
        "poll_interval_s": 1.0,
        "traffic": {},
        "apps": {"mode": "explicit_apps", "count": 0, "specs": []},
        "convergence": {"initial_converged_at_s": 1.2, "fault_reconvergence": []},
        "samples": [{"t_s": 0.0, "node_data": {}}],
    }
    mode, errors = module.validate_payload(payload, module.MODE_SCENARIO)
    assert mode == module.MODE_SCENARIO
    assert errors == []


def test_validate_scenario_accepts_ecmp_protocol() -> None:
    module = _load_module()
    payload = {
        "config_file": "/tmp/x.yaml",
        "lab_name": "demo",
        "protocol": "ecmp",
        "topology_file": "/tmp/t.clab.yaml",
        "duration_s": 10.0,
        "poll_interval_s": 1.0,
        "traffic": {},
        "apps": {"mode": "explicit_apps", "count": 0, "specs": []},
        "convergence": {"initial_converged_at_s": 1.2, "fault_reconvergence": []},
        "samples": [{"t_s": 0.0, "node_data": {}}],
    }
    mode, errors = module.validate_payload(payload, module.MODE_SCENARIO)
    assert mode == module.MODE_SCENARIO
    assert errors == []


def test_validate_scenario_accepts_topk_protocol() -> None:
    module = _load_module()
    payload = {
        "config_file": "/tmp/x.yaml",
        "lab_name": "demo",
        "protocol": "topk",
        "topology_file": "/tmp/t.clab.yaml",
        "duration_s": 10.0,
        "poll_interval_s": 1.0,
        "traffic": {},
        "apps": {"mode": "explicit_apps", "count": 0, "specs": []},
        "convergence": {"initial_converged_at_s": 1.2, "fault_reconvergence": []},
        "samples": [{"t_s": 0.0, "node_data": {}}],
    }
    mode, errors = module.validate_payload(payload, module.MODE_SCENARIO)
    assert mode == module.MODE_SCENARIO
    assert errors == []


def test_validate_scenario_accepts_dgr_protocol() -> None:
    module = _load_module()
    payload = {
        "config_file": "/tmp/x.yaml",
        "lab_name": "demo",
        "protocol": "dgr",
        "topology_file": "/tmp/t.clab.yaml",
        "duration_s": 10.0,
        "poll_interval_s": 1.0,
        "traffic": {},
        "apps": {"mode": "explicit_apps", "count": 0, "specs": []},
        "convergence": {"initial_converged_at_s": 1.2, "fault_reconvergence": []},
        "samples": [{"t_s": 0.0, "node_data": {}}],
    }
    mode, errors = module.validate_payload(payload, module.MODE_SCENARIO)
    assert mode == module.MODE_SCENARIO
    assert errors == []
