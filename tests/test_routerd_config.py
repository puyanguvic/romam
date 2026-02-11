from __future__ import annotations

from pathlib import Path

from rpf.runtime.config import load_daemon_config


def test_load_daemon_config_parses_protocol_and_forwarding(tmp_path: Path) -> None:
    cfg_path = tmp_path / "router.yaml"
    cfg_path.write_text(
        """
router_id: 7
protocol: ospf
bind:
  address: 0.0.0.0
  port: 5600
timers:
  tick_interval: 0.5
  dead_interval: 2.0
neighbors:
  - router_id: 8
    address: 10.0.78.8
    port: 5500
    cost: 3.5
protocol_params:
  ospf:
    hello_interval: 0.5
forwarding:
  enabled: true
  dry_run: true
  destination_prefixes:
    "8": 10.8.0.0/24
  next_hop_ips:
    "8": 10.0.78.8
""".strip(),
        encoding="utf-8",
    )
    cfg = load_daemon_config(cfg_path)

    assert cfg.router_id == 7
    assert cfg.protocol == "ospf"
    assert cfg.bind_port == 5600
    assert cfg.dead_interval == 2.0
    assert cfg.neighbors[0].router_id == 8
    assert cfg.protocol_params["hello_interval"] == 0.5
    assert cfg.forwarding.enabled is True
    assert cfg.forwarding.destination_prefixes[8] == "10.8.0.0/24"
