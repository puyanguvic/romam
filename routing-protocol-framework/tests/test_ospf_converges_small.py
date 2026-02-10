from __future__ import annotations

from rpf.backends.emu import EmuBackend


def test_ospf_converges_small(tmp_path):
    cfg = {
        "name": "ospf_small",
        "seed": 7,
        "protocol": "ospf_like",
        "protocol_params": {"spf_interval": 2, "lsa_refresh": 20, "jitter": 0},
        "topology": {"type": "ring", "n_nodes": 8, "default_metric": 1.0},
        "engine": {"max_ticks": 60},
        "network": {"base_delay": 1, "jitter": 0, "loss_prob": 0.0},
        "output_dir": str(tmp_path),
    }
    run = EmuBackend().run(cfg)

    assert run["converged_tick"] is not None
    n = 8
    for node_routes in run["route_tables"].values():
        assert len(node_routes) == n
