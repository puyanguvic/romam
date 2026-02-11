from __future__ import annotations

import json
from dataclasses import dataclass
from enum import Enum
from typing import Any, Dict


class MessageKind(str, Enum):
    HELLO = "hello"
    OSPF_LSA = "ospf_lsa"
    RIP_UPDATE = "rip_update"


@dataclass(frozen=True)
class ControlMessage:
    protocol: str
    kind: MessageKind
    src_router_id: int
    seq: int
    payload: Dict[str, Any]
    ts: float

    def to_dict(self) -> Dict[str, Any]:
        return {
            "protocol": self.protocol,
            "kind": self.kind.value,
            "src_router_id": int(self.src_router_id),
            "seq": int(self.seq),
            "payload": self.payload,
            "ts": float(self.ts),
        }


def encode_message(message: ControlMessage) -> bytes:
    return json.dumps(message.to_dict(), sort_keys=True, separators=(",", ":")).encode("utf-8")


def decode_message(data: bytes) -> ControlMessage:
    raw = json.loads(data.decode("utf-8"))
    return ControlMessage(
        protocol=str(raw["protocol"]),
        kind=MessageKind(str(raw["kind"])),
        src_router_id=int(raw["src_router_id"]),
        seq=int(raw["seq"]),
        payload=dict(raw.get("payload", {})),
        ts=float(raw.get("ts", 0.0)),
    )
