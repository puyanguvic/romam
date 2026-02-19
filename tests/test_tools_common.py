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


def test_summarize_neighbor_fast_state_from_metrics_map() -> None:
    module = _load_module()
    summary = module.summarize_neighbor_fast_state_from_metrics_map(
        {
            "r1": {
                "protocol_metrics": {
                    "neighbor_fast_state": {
                        "2": {
                            "queue_level": 1,
                            "interface_utilization": 0.6,
                            "delay_ms": 2.0,
                            "loss_rate": 0.01,
                        },
                        "3": {
                            "queue_level": 3,
                            "interface_utilization": 0.8,
                            "delay_ms": 5.0,
                            "loss_rate": 0.03,
                        },
                    }
                }
            },
            "r2": {
                "protocol_metrics": {
                    "neighbor_fast_state": {
                        "1": {
                            "queue_level": 2,
                            "interface_utilization": 0.5,
                            "delay_ms": 3.0,
                            "loss_rate": 0.02,
                        }
                    }
                }
            },
            "r3": {"protocol_metrics": {}},
        }
    )
    assert summary["sampled_nodes"] == 3
    assert summary["nodes_with_neighbor_fast_state"] == 2
    assert summary["neighbor_fast_state_entries"] == 3
    assert summary["avg_queue_level"] == 2.0
    assert summary["avg_interface_utilization"] == 0.6333333333333333
    assert summary["avg_delay_ms"] == 3.3333333333333335
    assert summary["avg_loss_rate"] == 0.02


def test_summarize_neighbor_fast_state_handles_missing_metrics() -> None:
    module = _load_module()
    summary = module.summarize_neighbor_fast_state_from_metrics_map(
        {"r1": {}, "r2": {"protocol_metrics": {"neighbor_fast_state": "bad"}}}
    )
    assert summary["sampled_nodes"] == 2
    assert summary["nodes_with_neighbor_fast_state"] == 0
    assert summary["neighbor_fast_state_entries"] == 0
    assert summary["avg_queue_level"] is None
