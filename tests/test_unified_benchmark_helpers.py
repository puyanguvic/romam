from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def _load_module():
    module_path = Path("scripts/run_unified_experiment.py").resolve()
    spec = importlib.util.spec_from_file_location("run_unified_experiment", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load scripts/run_unified_experiment.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_parse_ping_output_extracts_tx_rx_avg() -> None:
    module = _load_module()
    text = (
        "3 packets transmitted, 3 packets received, 0% packet loss\n"
        "rtt min/avg/max/mdev = 0.044/0.072/0.109/0.025 ms\n"
    )
    tx, rx, avg = module.parse_ping_output(text)
    assert tx == 3
    assert rx == 3
    assert avg == 0.072


def test_percentile_p95() -> None:
    module = _load_module()
    assert module.percentile([1.0, 2.0, 3.0, 10.0], 0.95) == 10.0


def test_append_runlab_generator_args() -> None:
    module = _load_module()
    cmd = ["python3", "scripts/run_routerd_lab.py"]
    cfg = {
        "lab": {
            "node_image": "ghcr.io/srl-labs/network-multitool:latest",
            "mgmt_network_name": "clab-mgmt-romam",
            "mgmt_ipv4_subnet": "10.250.10.0/24",
            "mgmt_ipv6_subnet": "fd00:fa:10::/64",
            "mgmt_external_access": True,
            "lab_name": "demo-lab",
        }
    }
    module.append_runlab_generator_args(cmd, cfg)
    text = " ".join(cmd)
    assert "--node-image ghcr.io/srl-labs/network-multitool:latest" in text
    assert "--mgmt-network-name clab-mgmt-romam" in text
    assert "--mgmt-ipv4-subnet 10.250.10.0/24" in text
    assert "--mgmt-ipv6-subnet fd00:fa:10::/64" in text
    assert "--mgmt-external-access" in text
    assert "--lab-name demo-lab" in text
