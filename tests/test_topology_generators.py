from __future__ import annotations

from topology.topology import Topology


def test_ring_topology_edge_count() -> None:
    topo = Topology.ring(8)
    assert len(topo.nodes()) == 8
    assert len(topo.edge_list()) == 8


def test_er_topology_is_seed_stable() -> None:
    cfg = {"type": "er", "n_nodes": 20, "p": 0.2, "default_metric": 1.0}
    topo_a = Topology.from_config(cfg, seed=123)
    topo_b = Topology.from_config(cfg, seed=123)

    edges_a = [(e.u, e.v, e.metric) for e in topo_a.edge_list()]
    edges_b = [(e.u, e.v, e.metric) for e in topo_b.edge_list()]
    assert edges_a == edges_b


def test_ba_topology_node_count() -> None:
    topo = Topology.from_config({"type": "ba", "n_nodes": 30, "m": 2}, seed=5)
    assert len(topo.nodes()) == 30
