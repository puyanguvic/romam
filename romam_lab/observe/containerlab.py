from __future__ import annotations

import json
from typing import Any, Dict, List

from romam_lab.traffic.containerlab import exec_cmd


def collect_routes_json(*, topo: str, node: str, table: int) -> List[Dict[str, Any]]:
    res = exec_cmd(topo=topo, node=node, cmd=f"ip -j -4 route show table {int(table)}", check=True, capture=True)
    raw = (res.stdout or "").strip()
    if not raw:
        return []
    return json.loads(raw)


def collect_addrs_json(*, topo: str, node: str) -> List[Dict[str, Any]]:
    res = exec_cmd(topo=topo, node=node, cmd="ip -j -4 addr show", check=True, capture=True)
    raw = (res.stdout or "").strip()
    if not raw:
        return []
    return json.loads(raw)


def collect_links_json(*, topo: str, node: str) -> List[Dict[str, Any]]:
    res = exec_cmd(topo=topo, node=node, cmd="ip -j -s link show", check=True, capture=True)
    raw = (res.stdout or "").strip()
    if not raw:
        return []
    return json.loads(raw)

