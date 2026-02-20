from __future__ import annotations

from pathlib import Path

from clab.clab_loader import load_clab_topology


def test_load_clab_topology_parses_real_ring_file() -> None:
    source = load_clab_topology(Path("src/clab/topologies/ring6.clab.yaml"))
    assert source.node_names == ["r1", "r2", "r3", "r4", "r5", "r6"]
    assert len(source.links) == 6
    first = source.links[0]
    assert first.left_node == "r1"
    assert first.left_iface == "eth1"
    assert first.left_ip == "10.11.1.1"
    assert first.right_node == "r2"
    assert first.right_ip == "10.11.1.2"


def test_load_clab_topology_parses_real_abilene_file() -> None:
    source = load_clab_topology(Path("src/clab/topologies/abilene.clab.yaml"))
    assert len(source.node_names) == 11
    assert len(source.links) == 14
    first = source.links[0]
    assert first.left_node == "new_york"
    assert first.left_iface == "eth1"
    assert first.left_ip == "10.15.1.1"
    assert first.right_node == "chicago"
    assert first.right_ip == "10.15.1.2"


def test_load_clab_topology_parses_real_geant_file() -> None:
    source = load_clab_topology(Path("src/clab/topologies/geant.clab.yaml"))
    assert len(source.node_names) == 40
    assert len(source.links) == 61
    first = source.links[0]
    assert first.left_node == "nl"
    assert first.left_iface == "eth1"
    assert first.left_ip == "10.16.1.1"
    assert first.right_node == "be"
    assert first.right_ip == "10.16.1.2"


def test_load_clab_topology_parses_real_uunet_file() -> None:
    source = load_clab_topology(Path("src/clab/topologies/uunet.clab.yaml"))
    assert len(source.node_names) == 49
    assert len(source.links) == 84
    first = source.links[0]
    assert first.left_node == "montreal"
    assert first.left_iface == "eth1"
    assert first.left_ip == "10.17.1.1"
    assert first.right_node == "halifax"
    assert first.right_ip == "10.17.1.2"


def test_load_clab_topology_parses_real_cernet_file() -> None:
    source = load_clab_topology(Path("src/clab/topologies/cernet.clab.yaml"))
    assert len(source.node_names) == 41
    assert len(source.links) == 59
    first = source.links[0]
    assert first.left_node == "gullin"
    assert first.left_iface == "eth1"
    assert first.left_ip == "10.18.1.1"
    assert first.right_node == "nanning"
    assert first.right_ip == "10.18.1.2"


def test_load_clab_topology_falls_back_when_link_ip_missing(tmp_path: Path) -> None:
    topology_file = tmp_path / "no-ip.clab.yaml"
    topology_file.write_text(
        """
name: x
topology:
  nodes:
    a: {kind: linux}
    b: {kind: linux}
  links:
    - endpoints: ["a:eth1", "b:eth1"]
""".strip(),
        encoding="utf-8",
    )
    source = load_clab_topology(topology_file)
    assert source.node_names == ["a", "b"]
    assert len(source.links) == 1
    assert source.links[0].left_ip == "10.0.0.1"
    assert source.links[0].right_ip == "10.0.0.2"
