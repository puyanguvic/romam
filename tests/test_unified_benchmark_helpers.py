from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path

import pytest


def _load_module():
    module_path = Path("tools/run_unified_experiment.py").resolve()
    spec = importlib.util.spec_from_file_location("run_unified_experiment", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Unable to load tools/run_unified_experiment.py")
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


def test_validate_protocol_accepts_octopus() -> None:
    module = _load_module()
    assert module.validate_protocol("octopus") == "octopus"


def test_append_runlab_generator_args() -> None:
    module = _load_module()
    cmd = ["python3", "tools/run_routerd_lab.py"]
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


def test_parse_run_routerd_lab_metadata_success() -> None:
    module = _load_module()
    output = (
        "topology_file: /tmp/demo.clab.yaml\n"
        "configs_dir: /tmp/demo_cfg\n"
        "deploy_env_file: /tmp/demo.env\n"
        "lab_name: demo-lab\n"
    )
    parsed = module.parse_run_routerd_lab_metadata(output, context="test")
    assert parsed["topology_file"] == "/tmp/demo.clab.yaml"
    assert parsed["configs_dir"] == "/tmp/demo_cfg"
    assert parsed["deploy_env_file"] == "/tmp/demo.env"
    assert parsed["lab_name"] == "demo-lab"


def test_parse_run_routerd_lab_metadata_missing_key() -> None:
    module = _load_module()
    with pytest.raises(RuntimeError):
        module.parse_run_routerd_lab_metadata(
            "topology_file: /tmp/demo.clab.yaml\nlab_name: demo-lab\n",
            context="test",
        )


def test_validate_protocol_rejects_irp() -> None:
    module = _load_module()
    with pytest.raises(ValueError):
        module.validate_protocol("irp")


def test_build_run_routerd_lab_cmd_includes_ecmp_params() -> None:
    module = _load_module()
    cmd = module.build_run_routerd_lab_cmd(
        protocol="ecmp",
        topology_key="profile",
        topology_value="line3",
        use_sudo=False,
        config={},
        precheck_min_routes=0,
        precheck_max_wait_s=20,
        precheck_poll_interval_s=1.0,
        precheck_tail_lines=120,
        ecmp_params={"hash_seed": 2026},
    )
    text = " ".join(cmd)
    assert "--protocol ecmp" in text
    assert "--ecmp-hash-seed 2026" in text


def test_build_run_routerd_lab_cmd_includes_topk_params() -> None:
    module = _load_module()
    cmd = module.build_run_routerd_lab_cmd(
        protocol="topk",
        topology_key="profile",
        topology_value="line3",
        use_sudo=False,
        config={},
        precheck_min_routes=0,
        precheck_max_wait_s=20,
        precheck_poll_interval_s=1.0,
        precheck_tail_lines=120,
        topk_params={
            "k_paths": 4,
            "explore_probability": 0.35,
            "selection_hold_time_s": 2.5,
            "rng_seed": 2026,
        },
    )
    text = " ".join(cmd)
    assert "--protocol topk" in text
    assert "--topk-k-paths 4" in text
    assert "--topk-explore-probability 0.35" in text
    assert "--topk-selection-hold-time-s 2.5" in text
    assert "--topk-rng-seed 2026" in text


def test_build_run_routerd_lab_cmd_includes_ddr_params() -> None:
    module = _load_module()
    cmd = module.build_run_routerd_lab_cmd(
        protocol="ddr",
        topology_key="profile",
        topology_value="line3",
        use_sudo=False,
        config={},
        precheck_min_routes=0,
        precheck_max_wait_s=20,
        precheck_poll_interval_s=1.0,
        precheck_tail_lines=120,
        ddr_params={
            "k_paths": 4,
            "deadline_ms": 80.0,
            "flow_size_bytes": 32768.0,
            "link_bandwidth_bps": 25000000.0,
            "queue_sample_interval": 0.5,
            "queue_levels": 4,
            "pressure_threshold": 2,
            "queue_level_scale_ms": 8.0,
            "randomize_route_selection": False,
            "rng_seed": 7,
        },
    )
    text = " ".join(cmd)
    assert "--protocol ddr" in text
    assert "--ddr-k-paths 4" in text
    assert "--ddr-deadline-ms 80.0" in text
    assert "--ddr-flow-size-bytes 32768.0" in text
    assert "--ddr-link-bandwidth-bps 25000000.0" in text
    assert "--ddr-queue-sample-interval 0.5" in text
    assert "--ddr-queue-levels 4" in text
    assert "--ddr-pressure-threshold 2" in text
    assert "--ddr-queue-level-scale-ms 8.0" in text
    assert "--ddr-rng-seed 7" in text
    assert "--no-ddr-randomized-selection" in text


def test_build_run_routerd_lab_cmd_includes_dgr_params() -> None:
    module = _load_module()
    cmd = module.build_run_routerd_lab_cmd(
        protocol="dgr",
        topology_key="profile",
        topology_value="line3",
        use_sudo=False,
        config={},
        precheck_min_routes=0,
        precheck_max_wait_s=20,
        precheck_poll_interval_s=1.0,
        precheck_tail_lines=120,
        ddr_params={
            "k_paths": 5,
            "deadline_ms": 70.0,
            "flow_size_bytes": 24576.0,
            "link_bandwidth_bps": 15000000.0,
            "queue_sample_interval": 0.2,
            "queue_levels": 4,
            "pressure_threshold": 2,
            "queue_level_scale_ms": 8.0,
            "randomize_route_selection": True,
            "rng_seed": 2026,
        },
    )
    text = " ".join(cmd)
    assert "--protocol dgr" in text
    assert "--ddr-k-paths 5" in text
    assert "--ddr-deadline-ms 70.0" in text
    assert "--ddr-flow-size-bytes 24576.0" in text
    assert "--ddr-link-bandwidth-bps 15000000.0" in text
    assert "--ddr-queue-sample-interval 0.2" in text
    assert "--ddr-queue-levels 4" in text
    assert "--ddr-pressure-threshold 2" in text
    assert "--ddr-queue-level-scale-ms 8.0" in text
    assert "--ddr-rng-seed 2026" in text
    assert "--ddr-randomized-selection" in text


def test_build_run_routerd_lab_cmd_includes_octopus_params() -> None:
    module = _load_module()
    cmd = module.build_run_routerd_lab_cmd(
        protocol="octopus",
        topology_key="profile",
        topology_value="line3",
        use_sudo=False,
        config={},
        precheck_min_routes=0,
        precheck_max_wait_s=20,
        precheck_poll_interval_s=1.0,
        precheck_tail_lines=120,
        ddr_params={
            "k_paths": 4,
            "deadline_ms": 1000000000.0,
            "flow_size_bytes": 24576.0,
            "link_bandwidth_bps": 15000000.0,
            "queue_sample_interval": 0.2,
            "queue_levels": 4,
            "pressure_threshold": 3,
            "queue_level_scale_ms": 8.0,
            "randomize_route_selection": True,
            "rng_seed": 2026,
        },
    )
    text = " ".join(cmd)
    assert "--protocol octopus" in text
    assert "--ddr-k-paths 4" in text
    assert "--ddr-deadline-ms 1000000000.0" in text
    assert "--ddr-flow-size-bytes 24576.0" in text
    assert "--ddr-link-bandwidth-bps 15000000.0" in text
    assert "--ddr-queue-sample-interval 0.2" in text
    assert "--ddr-queue-levels 4" in text
    assert "--ddr-pressure-threshold 3" in text
    assert "--ddr-queue-level-scale-ms 8.0" in text
    assert "--ddr-rng-seed 2026" in text
    assert "--ddr-randomized-selection" in text


def test_build_run_routerd_lab_cmd_includes_qdisc_params() -> None:
    module = _load_module()
    cmd = module.build_run_routerd_lab_cmd(
        protocol="ospf",
        topology_key="profile",
        topology_value="line3",
        use_sudo=False,
        config={},
        precheck_min_routes=0,
        precheck_max_wait_s=20,
        precheck_poll_interval_s=1.0,
        precheck_tail_lines=120,
        qdisc_params={
            "enabled": True,
            "dry_run": False,
            "default_kind": "prio",
            "default_handle": "1:",
            "default_parent": "",
            "default_params": {"bands": "3"},
        },
    )
    text = " ".join(cmd)
    assert "--qdisc-enabled" in text
    assert "--no-qdisc-dry-run" in text
    assert "--qdisc-default-kind prio" in text
    assert "--qdisc-default-handle 1:" in text
    assert "--qdisc-default-params-json" in text


def test_write_standard_run_artifacts(tmp_path: Path) -> None:
    module = _load_module()
    topology_file = tmp_path / "demo.clab.yaml"
    topology_file.write_text("name: demo\n", encoding="utf-8")

    out_dir = module.write_standard_run_artifacts(
        run_dir=tmp_path / "run_demo",
        input_config={"protocol": "ospf"},
        topology_file=topology_file,
        traffic_payload={"mode": "scenario"},
        metrics_payload={"ok": True},
        summary_lines=["- mode: scenario"],
    )
    assert (out_dir / "config.yaml").is_file()
    assert (out_dir / "topology.yaml").is_file()
    assert (out_dir / "traffic.yaml").is_file()
    assert (out_dir / "logs").is_dir()
    assert (out_dir / "metrics.json").is_file()
    assert (out_dir / "summary.md").is_file()

    metrics = json.loads((out_dir / "metrics.json").read_text(encoding="utf-8"))
    assert metrics["ok"] is True
