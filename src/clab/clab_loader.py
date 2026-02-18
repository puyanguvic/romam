from __future__ import annotations

import ipaddress
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import yaml

IP_ADDR_RE = re.compile(r"\bip\s+addr\s+replace\s+([0-9A-Fa-f:.]+)/\d+\s+dev\s+(\S+)\b")


@dataclass(frozen=True)
class ClabLink:
    left_node: str
    left_iface: str
    left_ip: str
    right_node: str
    right_iface: str
    right_ip: str
    cost: float = 1.0


@dataclass(frozen=True)
class ClabTopology:
    source_path: Path
    raw: Dict[str, Any]
    node_names: List[str]
    links: List[ClabLink]


def load_clab_topology(path: Path) -> ClabTopology:
    source_path = path.expanduser().resolve()
    with source_path.open("r", encoding="utf-8") as f:
        raw = yaml.safe_load(f) or {}

    topology = dict(raw.get("topology", {}))
    nodes = dict(topology.get("nodes", {}))
    links_raw = list(topology.get("links", []))
    if not nodes:
        raise ValueError(f"No nodes found in topology file: {source_path}")
    if not links_raw:
        raise ValueError(f"No links found in topology file: {source_path}")

    node_names = [str(name) for name in nodes.keys()]
    iface_ips: Dict[str, Dict[str, str]] = {
        node_name: _parse_iface_ips(nodes[node_name]) for node_name in node_names
    }

    links: List[ClabLink] = []
    for idx, link in enumerate(links_raw):
        endpoints = list(dict(link).get("endpoints", []))
        if len(endpoints) != 2:
            raise ValueError(f"Link entry #{idx} must contain exactly two endpoints: {link}")
        left_node, left_iface = _parse_endpoint(str(endpoints[0]))
        right_node, right_iface = _parse_endpoint(str(endpoints[1]))
        if left_node not in nodes or right_node not in nodes:
            raise ValueError(f"Unknown node in link entry #{idx}: {link}")

        left_ip = iface_ips.get(left_node, {}).get(left_iface, "")
        right_ip = iface_ips.get(right_node, {}).get(right_iface, "")
        if not left_ip and right_ip:
            left_ip = _peer_ip(right_ip)
        if not right_ip and left_ip:
            right_ip = _peer_ip(left_ip)
        if not left_ip and not right_ip:
            left_ip, right_ip = _edge_ip_pair(idx)

        links.append(
            ClabLink(
                left_node=left_node,
                left_iface=left_iface,
                left_ip=left_ip,
                right_node=right_node,
                right_iface=right_iface,
                right_ip=right_ip,
                cost=float(dict(link).get("cost", dict(link).get("metric", 1.0))),
            )
        )

    return ClabTopology(
        source_path=source_path,
        raw=raw,
        node_names=node_names,
        links=links,
    )


def _parse_iface_ips(node: Dict[str, Any]) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for cmd in list(node.get("exec", [])):
        text = str(cmd)
        match = IP_ADDR_RE.search(text)
        if not match:
            continue
        out[match.group(2)] = match.group(1)
    return out


def _parse_endpoint(text: str) -> tuple[str, str]:
    if ":" not in text:
        raise ValueError(f"Invalid endpoint format: {text}")
    node, iface = text.split(":", maxsplit=1)
    if not node or not iface:
        raise ValueError(f"Invalid endpoint format: {text}")
    return node, iface


def _peer_ip(ip: str) -> str:
    iface = ipaddress.ip_interface(f"{ip}/30")
    hosts = [str(host) for host in iface.network.hosts()]
    if len(hosts) != 2:
        raise ValueError(f"Only /30 peer inference is supported, got ip={ip}")
    return hosts[1] if hosts[0] == ip else hosts[0]


def _edge_ip_pair(edge_idx: int) -> tuple[str, str]:
    second_octet = edge_idx // 256
    third_octet = edge_idx % 256
    if second_octet > 255:
        raise ValueError("Too many edges for /30 IP allocation plan (max 65536 links).")
    subnet = f"10.{second_octet}.{third_octet}"
    return f"{subnet}.1", f"{subnet}.2"
