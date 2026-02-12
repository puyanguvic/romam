from __future__ import annotations

from pathlib import Path

import yaml

from rpf.topology.labgen import LabGenParams, generate_routerd_lab


def test_generate_routerd_lab_outputs_topology_and_configs(tmp_path: Path) -> None:
    out_dir = tmp_path / "lab"
    params = LabGenParams(
        protocol="ospf",
        topology_type="ring",
        n_nodes=4,
        seed=1,
        er_p=0.2,
        ba_m=2,
        rows=2,
        cols=2,
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
        source_dir=Path.cwd() / "src",
        mgmt_network_name="testlab-mgmt",
        mgmt_ipv4_subnet="10.250.10.0/24",
        mgmt_ipv6_subnet="fd00:fa:10::/64",
        mgmt_external_access=False,
    )

    result = generate_routerd_lab(params)
    topology_path = Path(result["topology_file"])
    configs_dir = Path(result["configs_dir"])

    assert topology_path.is_file()
    assert configs_dir.is_dir()

    cfg_files = sorted(configs_dir.glob("r*.yaml"))
    assert len(cfg_files) == 4

    topo = yaml.safe_load(topology_path.read_text(encoding="utf-8"))
    assert topo["name"] == "testlab"
    assert topo["mgmt"]["network"] == "testlab-mgmt"
    assert len(topo["topology"]["nodes"]) == 4
    assert len(topo["topology"]["links"]) == 4

    node_r1 = topo["topology"]["nodes"]["r1"]
    exec_lines = node_r1["exec"]
    assert any("python3 -m rpf.routerd" in line for line in exec_lines)
    assert any("ip addr replace" in line for line in exec_lines)

    cfg_r1 = yaml.safe_load((configs_dir / "r1.yaml").read_text(encoding="utf-8"))
    assert cfg_r1["router_id"] == 1
    assert cfg_r1["protocol"] == "ospf"
    assert cfg_r1["neighbors"]
