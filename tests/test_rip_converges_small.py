from __future__ import annotations

from rpf.backends.emu import EmuBackend


def test_rip_converges_small(tmp_path):
    cfg = {
        "name": "rip_small",
        "seed": 9,
        "protocol": "rip_like",
        "protocol_params": {
            "update_interval": 1,
            "triggered_min_gap": 1,
            "infinity_metric": 64,
            "split_horizon": True,
        },
        "topology": {"type": "ring", "n_nodes": 6, "default_metric": 1.0},
        "engine": {"max_ticks": 80},
        "network": {"base_delay": 1, "jitter": 0, "loss_prob": 0.0},
        "output_dir": str(tmp_path),
    }
    run = EmuBackend().run(cfg)

    assert run["converged_tick"] is not None
    n = 6
    for node_routes in run["route_tables"].values():
        assert len(node_routes) == n
