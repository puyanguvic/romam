from __future__ import annotations

from typing import Dict, Type

from rpf.protocols.base import RoutingProtocol
from rpf.protocols.ecmp import EcmpProtocol, EcompProtocol
from rpf.protocols.ospf import OspfProtocol
from rpf.protocols.rip import RipProtocol

_REGISTRY: Dict[str, Type[RoutingProtocol]] = {
    "ospf": OspfProtocol,
    "rip": RipProtocol,
    "ecmp": EcmpProtocol,
    "ecomp": EcompProtocol,
}


def register_protocol(name: str, protocol_cls: Type[RoutingProtocol]) -> None:
    _REGISTRY[str(name)] = protocol_cls


def load_protocol(name: str) -> Type[RoutingProtocol]:
    if name not in _REGISTRY:
        raise KeyError(f"Unknown protocol: {name}. Available: {sorted(_REGISTRY.keys())}")
    return _REGISTRY[name]


def available_protocols() -> list[str]:
    return sorted(_REGISTRY.keys())
