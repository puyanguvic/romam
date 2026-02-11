from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List

from rpf.utils.io import load_yaml


@dataclass(frozen=True)
class NeighborConfig:
    router_id: int
    address: str
    port: int
    cost: float = 1.0


@dataclass(frozen=True)
class ForwardingConfig:
    enabled: bool = False
    dry_run: bool = True
    table: int = 254
    destination_prefixes: Dict[int, str] = field(default_factory=dict)
    next_hop_ips: Dict[int, str] = field(default_factory=dict)


@dataclass(frozen=True)
class DaemonConfig:
    router_id: int
    protocol: str
    bind_address: str
    bind_port: int
    tick_interval: float
    dead_interval: float
    neighbors: List[NeighborConfig]
    protocol_params: Dict[str, Any]
    forwarding: ForwardingConfig


def load_daemon_config(path: str | Path) -> DaemonConfig:
    raw = load_yaml(path)
    bind = dict(raw.get("bind", {}))
    timers = dict(raw.get("timers", {}))
    forwarding_raw = dict(raw.get("forwarding", {}))

    protocol = str(raw.get("protocol", "ospf")).lower()
    protocol_params_all = dict(raw.get("protocol_params", {}))
    protocol_params = dict(protocol_params_all.get(protocol, {}))

    neighbors = [
        NeighborConfig(
            router_id=int(item["router_id"]),
            address=str(item["address"]),
            port=int(item.get("port", 5500)),
            cost=float(item.get("cost", 1.0)),
        )
        for item in raw.get("neighbors", [])
    ]

    forwarding = ForwardingConfig(
        enabled=bool(forwarding_raw.get("enabled", False)),
        dry_run=bool(forwarding_raw.get("dry_run", True)),
        table=int(forwarding_raw.get("table", 254)),
        destination_prefixes=_parse_int_key_map(forwarding_raw.get("destination_prefixes", {})),
        next_hop_ips=_parse_int_key_map(forwarding_raw.get("next_hop_ips", {})),
    )

    return DaemonConfig(
        router_id=int(raw["router_id"]),
        protocol=protocol,
        bind_address=str(bind.get("address", raw.get("bind_address", "0.0.0.0"))),
        bind_port=int(bind.get("port", raw.get("bind_port", 5500))),
        tick_interval=float(timers.get("tick_interval", 1.0)),
        dead_interval=float(timers.get("dead_interval", 4.0)),
        neighbors=neighbors,
        protocol_params=protocol_params,
        forwarding=forwarding,
    )


def _parse_int_key_map(raw: Dict[Any, Any]) -> Dict[int, str]:
    out: Dict[int, str] = {}
    for key, value in dict(raw).items():
        out[int(key)] = str(value)
    return out
