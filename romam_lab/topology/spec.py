from __future__ import annotations

import ipaddress
import json
import os
from typing import Any, Dict, Iterator, Tuple

try:
    import tomllib  # py3.11+
except Exception:  # pragma: no cover
    tomllib = None

try:  # optional backport for py<3.11
    import tomli  # type: ignore
except Exception:  # pragma: no cover
    tomli = None


def load_spec(path: str) -> Dict[str, Any]:
    with open(path, "rb") as f:
        raw = f.read()
    if path.endswith(".toml"):
        if tomllib is not None:
            return tomllib.loads(raw.decode("utf-8"))
        if tomli is not None:
            return tomli.loads(raw.decode("utf-8"))
        raise ValueError("TOML spec requires Python 3.11+ or install 'tomli' (or use .json)")
    if path.endswith(".json"):
        return json.loads(raw.decode("utf-8"))
    raise ValueError("spec must be .toml or .json")


def _require(obj: Dict[str, Any], key: str, typ):
    if key not in obj:
        raise ValueError(f"missing required field: {key}")
    if not isinstance(obj[key], typ):
        raise ValueError(f"field '{key}' must be {typ.__name__}")
    return obj[key]


def _optional(obj: Dict[str, Any], key: str, typ, default=None):
    if key not in obj:
        return default
    if not isinstance(obj[key], typ):
        raise ValueError(f"field '{key}' must be {typ.__name__}")
    return obj[key]


def parse_multicast(s: str) -> Tuple[str, int]:
    if ":" not in s:
        raise ValueError("romam.multicast must be 'ip:port'")
    ip_s, port_s = s.rsplit(":", 1)
    ipaddress.IPv4Address(ip_s)
    port = int(port_s)
    if port < 1 or port > 65535:
        raise ValueError("multicast port out of range")
    return ip_s, port


def compute_router_id(loopback_base: str, node_id: int) -> str:
    net = ipaddress.IPv4Network(loopback_base, strict=True)
    if net.prefixlen > 24:
        raise ValueError("addressing.loopback_base prefix too small")
    host = node_id + 1
    addr_int = int(net.network_address) + host
    if addr_int >= int(net.broadcast_address):
        raise ValueError(f"node id out of range for loopback_base: {node_id}")
    return str(ipaddress.IPv4Address(addr_int))


def link_subnet_allocator(link_base: str, prefixlen: int) -> Iterator[ipaddress.IPv4Network]:
    net = ipaddress.IPv4Network(link_base, strict=True)
    if prefixlen < net.prefixlen:
        raise ValueError("link_prefixlen must be >= link_base prefixlen")
    subnet_size = 1 << (32 - prefixlen)
    cursor = int(net.network_address)
    end = int(net.broadcast_address) + 1
    while cursor + subnet_size <= end:
        yield ipaddress.IPv4Network((cursor, prefixlen))
        cursor += subnet_size


def validate_and_normalize(spec: Dict[str, Any]) -> Dict[str, Any]:
    version = _require(spec, "version", int)
    if version != 1:
        raise ValueError("unsupported spec version")

    romam = spec.get("romam", {}) or {}
    if not isinstance(romam, dict):
        raise ValueError("romam must be a table/object")
    multicast = _optional(romam, "multicast", str, "239.255.0.1:5000")
    _ = parse_multicast(multicast)
    routing_algo = _optional(romam, "routing_algo", str, "spf")
    if not routing_algo.strip():
        raise ValueError("romam.routing_algo must be a non-empty string")

    addressing = spec.get("addressing", {}) or {}
    if not isinstance(addressing, dict):
        raise ValueError("addressing must be a table/object")
    loopback_base = _optional(addressing, "loopback_base", str, "10.255.0.0/16")
    link_base = _optional(addressing, "link_base", str, "10.0.0.0/8")
    link_prefixlen = int(_optional(addressing, "link_prefixlen", int, 30))
    ipaddress.IPv4Network(loopback_base, strict=True)
    ipaddress.IPv4Network(link_base, strict=True)
    if link_prefixlen < 16 or link_prefixlen > 30:
        raise ValueError("addressing.link_prefixlen must be 16..30")

    defaults = spec.get("defaults", {}) or {}
    if not isinstance(defaults, dict):
        raise ValueError("defaults must be a table/object")
    default_cost = int(_optional(defaults, "cost", int, 1))
    if default_cost < 1:
        raise ValueError("defaults.cost must be >= 1")
    default_tc = defaults.get("tc", {}) or {}
    if not isinstance(default_tc, dict):
        raise ValueError("defaults.tc must be a table/object")

    nodes = _require(spec, "nodes", list)
    links = _require(spec, "links", list)

    node_by_name: Dict[str, Dict[str, Any]] = {}
    for n in nodes:
        if not isinstance(n, dict):
            raise ValueError("each node must be a table/object")
        name = _require(n, "name", str)
        if name in node_by_name:
            raise ValueError(f"duplicate node name: {name}")
        node_id = int(_require(n, "id", int))
        if node_id < 0:
            raise ValueError(f"node id must be >=0: {name}")
        router_id = _optional(n, "router_id", str, None)
        if router_id is None:
            router_id = compute_router_id(loopback_base, node_id)
        else:
            ipaddress.IPv4Address(router_id)
        advertise = _optional(n, "advertise", list, [])
        if not all(isinstance(x, str) for x in advertise):
            raise ValueError(f"node.advertise must be list of strings: {name}")
        node_by_name[name] = {
            "name": name,
            "id": node_id,
            "router_id": router_id,
            "advertise": advertise,
        }

    norm_links = []
    seen = set()
    for e in links:
        if not isinstance(e, dict):
            raise ValueError("each link must be a table/object")
        a = _require(e, "a", str)
        b = _require(e, "b", str)
        if a not in node_by_name or b not in node_by_name:
            raise ValueError(f"link endpoint not found: {a} {b}")
        if a == b:
            raise ValueError("self-loop not allowed")
        key = tuple(sorted([a, b]))
        if key in seen:
            raise ValueError(f"duplicate link: {key[0]} {key[1]}")
        seen.add(key)
        cost = int(_optional(e, "cost", int, default_cost))
        if cost < 1:
            raise ValueError(f"link cost must be >=1: {a}-{b}")
        tc = e.get("tc", {}) or {}
        if not isinstance(tc, dict):
            raise ValueError("link.tc must be a table/object")
        link_tc = dict(default_tc)
        link_tc.update(tc)
        norm_links.append({"a": a, "b": b, "cost": cost, "tc": link_tc})

    def _default_name() -> str:
        n = spec.get("name")
        if isinstance(n, str) and n.strip():
            return n.strip()
        return os.path.basename("spec")

    return {
        "name": _optional(spec, "name", str, _default_name()),
        "romam": {
            "multicast": multicast,
            "routing_algo": routing_algo,
            "hello_interval_ms": int(_optional(romam, "hello_interval_ms", int, 500)),
            "dead_interval_ms": int(_optional(romam, "dead_interval_ms", int, 2000)),
            "lsa_interval_ms": int(_optional(romam, "lsa_interval_ms", int, 1000)),
            "route_table": int(_optional(romam, "route_table", int, 100)),
            "route_metric_base": int(_optional(romam, "route_metric_base", int, 100)),
        },
        "addressing": {
            "loopback_base": loopback_base,
            "link_base": link_base,
            "link_prefixlen": link_prefixlen,
        },
        "nodes": [node_by_name[k] for k in sorted(node_by_name.keys())],
        "node_by_name": node_by_name,
        "links": norm_links,
    }
