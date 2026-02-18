from __future__ import annotations

from pathlib import Path

import yaml

from topology.labgen import LabGenParams, generate_routerd_lab


def test_generate_routerd_lab_outputs_configs_and_env(tmp_path: Path) -> None:
    out_dir = tmp_path / "lab"
    params = LabGenParams(
        protocol="ospf",
        routing_alpha=1.0,
        routing_beta=2.0,
        topology_file=Path("clab_topologies/ring6.clab.yaml"),
        node_image="ghcr.io/srl-labs/network-multitool:latest",
        bind_port=5500,
        tick_interval=1.0,
        dead_interval=4.0,
        ospf_hello_interval=1.0,
        ospf_lsa_interval=3.0,
        ospf_lsa_max_age=15.0,
        rip_update_interval=5.0,
        rip_neighbor_timeout=15.0,
        rip_infinity_metric=16.0,
        rip_poison_reverse=True,
        output_dir=out_dir,
        lab_name="testlab",
        log_level="INFO",
        mgmt_network_name="testlab-mgmt",
        mgmt_ipv4_subnet="10.250.10.0/24",
        mgmt_ipv6_subnet="fd00:fa:10::/64",
        mgmt_external_access=False,
        forwarding_enabled=False,
        forwarding_dry_run=True,
    )

    result = generate_routerd_lab(params)
    source_topology_path = Path(result["topology_file"])
    configs_dir = Path(result["configs_dir"])
    deploy_env_file = Path(result["deploy_env_file"])

    assert source_topology_path.name == "ring6.clab.yaml"
    assert source_topology_path.parent.name == "clab_topologies"
    assert configs_dir.is_dir()
    assert deploy_env_file.is_file()

    cfg_files = sorted(path.name for path in configs_dir.glob("*.yaml"))
    assert cfg_files == ["r1.yaml", "r2.yaml", "r3.yaml", "r4.yaml", "r5.yaml", "r6.yaml"]

    env_lines = deploy_env_file.read_text(encoding="utf-8").splitlines()
    assert "CLAB_LAB_NAME=testlab" in env_lines
    assert "CLAB_NODE_IMAGE=ghcr.io/srl-labs/network-multitool:latest" in env_lines
    assert "CLAB_MGMT_NETWORK=testlab-mgmt" in env_lines
    assert "CLAB_MGMT_IPV4_SUBNET=10.250.10.0/24" in env_lines
    assert "CLAB_MGMT_IPV6_SUBNET=fd00:fa:10::/64" in env_lines
    assert "ROMAM_LOG_LEVEL=INFO" in env_lines

    cfg_r1 = yaml.safe_load((configs_dir / "r1.yaml").read_text(encoding="utf-8"))
    assert cfg_r1["router_id"] == 1
    assert cfg_r1["protocol"] == "ospf"
    assert cfg_r1["neighbors"][0]["address"] == "10.11.1.2"
    assert cfg_r1["management"]["http"]["enabled"] is True
    assert cfg_r1["management"]["grpc"]["enabled"] is True
    assert cfg_r1["management"]["http"]["port"] == 18001


def test_generate_routerd_lab_supports_named_nodes_with_rip(tmp_path: Path) -> None:
    out_dir = tmp_path / "lab-rip-spineleaf"
    params = LabGenParams(
        protocol="rip",
        routing_alpha=1.0,
        routing_beta=2.0,
        topology_file=Path("clab_topologies/spineleaf2x4.clab.yaml"),
        node_image="ghcr.io/srl-labs/network-multitool:latest",
        bind_port=5500,
        tick_interval=1.0,
        dead_interval=4.0,
        ospf_hello_interval=1.0,
        ospf_lsa_interval=3.0,
        ospf_lsa_max_age=15.0,
        rip_update_interval=5.0,
        rip_neighbor_timeout=15.0,
        rip_infinity_metric=16.0,
        rip_poison_reverse=True,
        output_dir=out_dir,
        lab_name="testlab-rip-spineleaf",
        log_level="INFO",
        mgmt_network_name="testlab-rip-spineleaf-mgmt",
        mgmt_ipv4_subnet="10.250.11.0/24",
        mgmt_ipv6_subnet="fd00:fa:11::/64",
        mgmt_external_access=False,
        forwarding_enabled=False,
        forwarding_dry_run=True,
    )

    result = generate_routerd_lab(params)
    configs_dir = Path(result["configs_dir"])
    deploy_env_file = Path(result["deploy_env_file"])

    assert Path(result["topology_file"]).name == "spineleaf2x4.clab.yaml"
    env_lines = deploy_env_file.read_text(encoding="utf-8").splitlines()
    assert "CLAB_LAB_NAME=testlab-rip-spineleaf" in env_lines

    cfg_s1 = yaml.safe_load((configs_dir / "s1.yaml").read_text(encoding="utf-8"))
    assert cfg_s1["protocol"] == "rip"
    assert "rip" in cfg_s1["protocol_params"]
    assert len(cfg_s1["neighbors"]) == 4


def test_generate_routerd_lab_can_enable_forwarding_config(tmp_path: Path) -> None:
    out_dir = tmp_path / "lab-forwarding"
    params = LabGenParams(
        protocol="ospf",
        routing_alpha=1.0,
        routing_beta=2.0,
        topology_file=Path("clab_topologies/line5.clab.yaml"),
        node_image="ghcr.io/srl-labs/network-multitool:latest",
        bind_port=5500,
        tick_interval=1.0,
        dead_interval=4.0,
        ospf_hello_interval=1.0,
        ospf_lsa_interval=3.0,
        ospf_lsa_max_age=15.0,
        rip_update_interval=5.0,
        rip_neighbor_timeout=15.0,
        rip_infinity_metric=16.0,
        rip_poison_reverse=True,
        output_dir=out_dir,
        lab_name="testlab-forwarding",
        log_level="INFO",
        mgmt_network_name="testlab-forwarding-mgmt",
        mgmt_ipv4_subnet="10.250.12.0/24",
        mgmt_ipv6_subnet="fd00:fa:12::/64",
        mgmt_external_access=False,
        forwarding_enabled=True,
        forwarding_dry_run=False,
    )

    result = generate_routerd_lab(params)
    cfg_r1 = yaml.safe_load(
        Path(result["configs_dir"]).joinpath("r1.yaml").read_text(encoding="utf-8")
    )
    forwarding = dict(cfg_r1["forwarding"])
    assert forwarding["enabled"] is True
    assert forwarding["dry_run"] is False
    assert forwarding["destination_prefixes"][5].endswith("/32")
    assert set(forwarding["next_hop_ips"].keys()) == {2}
