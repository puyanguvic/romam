from __future__ import annotations

from rpf.backends.emu import EmuBackend


def build_cfg(tmp_path):
    return {
        "name": "deterministic_order",
        "seed": 123,
        "protocol": "ospf_like",
        "protocol_params": {"spf_interval": 2, "lsa_refresh": 30, "jitter": 0},
        "topology": {"type": "ring", "n_nodes": 6, "default_metric": 1.0},
        "engine": {"max_ticks": 40},
        "network": {"base_delay": 1, "jitter": 0, "loss_prob": 0.0},
        "output_dir": str(tmp_path),
        "failures": [
            {"tick": 10, "action": "remove_link", "u": 1, "v": 2},
            {"tick": 20, "action": "add_link", "u": 1, "v": 2, "metric": 1.0},
        ],
    }


def test_tick_engine_deterministic_order(tmp_path):
    backend = EmuBackend()
    cfg = build_cfg(tmp_path)

    run1 = backend.run(cfg)
    run2 = backend.run(cfg)

    assert run1["route_hashes"] == run2["route_hashes"]
    assert run1["route_tables"] == run2["route_tables"]
