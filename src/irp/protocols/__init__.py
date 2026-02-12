"""Routing protocol engines."""

from irp.protocols.base import ProtocolContext, ProtocolEngine, ProtocolOutputs, RouterLink
from irp.protocols.ospf import OspfProtocol
from irp.protocols.rip import RipProtocol

__all__ = [
    "ProtocolContext",
    "ProtocolEngine",
    "ProtocolOutputs",
    "RouterLink",
    "OspfProtocol",
    "RipProtocol",
]
