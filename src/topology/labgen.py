from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Tuple

import yaml

from irp.utils.io import ensure_dir
from topology.topology import Topology


@dataclass(frozen=True)
class LabGenParams:
    protocol: str
    topology_type: str
    n_nodes: int
    seed: int
    er_p: float
    ba_m: int
    rows: int
    cols: int
    node_image: str
    bind_port: int
    tick_interval: float
    dead_interval: float
    ospf_hello_interval: float
    ospf_lsa_interval: float
    ospf_lsa_max_age: float
    rip_update_interval: float
    rip_neighbor_timeout: float
    rip_infinity_metric: float
    rip_poison_reverse: bool
    output_dir: Path
    lab_name: str
    log_level: str
    source_dir: Path
    mgmt_network_name: str
    mgmt_ipv4_subnet: str
    mgmt_ipv6_subnet: str
    mgmt_external_access: bool


def generate_routerd_lab(params: LabGenParams) -> Dict[str, str]:
    try:
        topology = _build_topology(params)
        output_dir = ensure_dir(params.output_dir)
        configs_dir = ensure_dir(output_dir / "configs")

        node_ids = sorted(topology.nodes())
        rid_map = {node_id: node_id + 1 for node_id in node_ids}
        iface_index = {node_id: 1 for node_id in node_ids}

        links: List[Dict[str, Any]] = []
        iface_plans: Dict[int, List[Dict[str, Any]]] = {node_id: [] for node_id in node_ids}

        for edge_idx, edge in enumerate(topology.edge_list()):
            u = edge.u
            v = edge.v
            iface_u = f"eth{iface_index[u]}"
            iface_v = f"eth{iface_index[v]}"
            iface_index[u] += 1
            iface_index[v] += 1

            ip_u, ip_v = _edge_ip_pair(edge_idx)
            links.append({"endpoints": [f"r{rid_map[u]}:{iface_u}", f"r{rid_map[v]}:{iface_v}"]})

            iface_plans[u].append(
                {
                    "iface": iface_u,
                    "local_ip": ip_u,
                    "neighbor_ip": ip_v,
                    "neighbor_id": rid_map[v],
                    "cost": float(edge.metric),
                }
            )
            iface_plans[v].append(
                {
                    "iface": iface_v,
                    "local_ip": ip_v,
                    "neighbor_ip": ip_u,
                    "neighbor_id": rid_map[u],
                    "cost": float(edge.metric),
                }
            )

        for node_id in node_ids:
            rid = rid_map[node_id]
            cfg = _build_routerd_config(params, rid, iface_plans[node_id])
            config_path = configs_dir / f"r{rid}.yaml"
            with config_path.open("w", encoding="utf-8") as f:
                yaml.safe_dump(cfg, f, sort_keys=False, width=4096)

        topology_data = _build_clab_topology(
            params=params,
            node_ids=node_ids,
            rid_map=rid_map,
            iface_plans=iface_plans,
            links=links,
            configs_dir=configs_dir,
        )
        topology_path = output_dir / f"{params.lab_name}.clab.yaml"
        with topology_path.open("w", encoding="utf-8") as f:
            yaml.safe_dump(topology_data, f, sort_keys=False, width=4096)

        return {
            "topology_file": str(topology_path),
            "configs_dir": str(configs_dir),
            "lab_name": params.lab_name,
        }
    except PermissionError as exc:
        output_base = params.output_dir
        raise PermissionError(
            f"Permission denied while writing lab assets under '{output_base}'. "
            "This is usually caused by previous root-owned files. "
            "Run: sudo chown -R $USER:$USER results/runs/routerd_labs"
        ) from exc


def _build_topology(params: LabGenParams) -> Topology:
    cfg: Dict[str, Any] = {
        "type": params.topology_type,
        "n_nodes": params.n_nodes,
        "default_metric": 1.0,
    }
    if params.topology_type == "er":
        cfg["p"] = params.er_p
    elif params.topology_type == "ba":
        cfg["m"] = params.ba_m
    elif params.topology_type == "grid":
        cfg["rows"] = params.rows
        cfg["cols"] = params.cols
    return Topology.from_config(cfg, seed=params.seed)


def _build_routerd_config(
    params: LabGenParams,
    router_id: int,
    local_ifaces: List[Dict[str, Any]],
) -> Dict[str, Any]:
    neighbors = [
        {
            "router_id": int(item["neighbor_id"]),
            "address": str(item["neighbor_ip"]),
            "port": int(params.bind_port),
            "cost": float(item["cost"]),
        }
        for item in sorted(local_ifaces, key=lambda x: int(x["neighbor_id"]))
    ]
    cfg: Dict[str, Any] = {
        "router_id": int(router_id),
        "protocol": params.protocol,
        "bind": {"address": "0.0.0.0", "port": int(params.bind_port)},
        "timers": {
            "tick_interval": float(params.tick_interval),
            "dead_interval": float(params.dead_interval),
        },
        "neighbors": neighbors,
        "forwarding": {"enabled": False, "dry_run": True},
    }
    if params.protocol == "ospf":
        cfg["protocol_params"] = {
            "ospf": {
                "hello_interval": float(params.ospf_hello_interval),
                "lsa_interval": float(params.ospf_lsa_interval),
                "lsa_max_age": float(params.ospf_lsa_max_age),
            }
        }
    else:
        cfg["protocol_params"] = {
            "rip": {
                "update_interval": float(params.rip_update_interval),
                "neighbor_timeout": float(params.rip_neighbor_timeout),
                "infinity_metric": float(params.rip_infinity_metric),
                "poison_reverse": bool(params.rip_poison_reverse),
            }
        }
    return cfg


def _build_clab_topology(
    params: LabGenParams,
    node_ids: List[int],
    rid_map: Dict[int, int],
    iface_plans: Dict[int, List[Dict[str, Any]]],
    links: List[Dict[str, Any]],
    configs_dir: Path,
) -> Dict[str, Any]:
    nodes: Dict[str, Dict[str, Any]] = {}
    src_bind = str(params.source_dir.resolve())
    configs_bind = str(configs_dir.resolve())

    for node_id in node_ids:
        rid = rid_map[node_id]
        node_name = f"r{rid}"
        exec_cmds = []
        for item in sorted(iface_plans[node_id], key=lambda x: _iface_index(str(x["iface"]))):
            iface = str(item["iface"])
            local_ip = str(item["local_ip"])
            exec_cmds.append(f"ip link set {iface} up")
            exec_cmds.append(f"ip addr replace {local_ip}/30 dev {iface}")
        exec_cmds.append("sysctl -w net.ipv4.ip_forward=1")
        daemon_cmd = (
            "sh -lc "
            f"'PYTHONPATH=/irp/src nohup python3 -m irp.routerd "
            f"--config /irp/configs/{node_name}.yaml "
            f"--log-level {params.log_level} >/tmp/routerd.log 2>&1 &'"
        )
        exec_cmds.append(
            daemon_cmd
        )
        nodes[node_name] = {
            "kind": "linux",
            "binds": [f"{src_bind}:/irp/src:ro", f"{configs_bind}:/irp/configs:ro"],
            "exec": exec_cmds,
        }

    return {
        "name": params.lab_name,
        "mgmt": {
            "network": params.mgmt_network_name,
            "ipv4-subnet": params.mgmt_ipv4_subnet,
            "ipv6-subnet": params.mgmt_ipv6_subnet,
            "external-access": bool(params.mgmt_external_access),
        },
        "topology": {
            "kinds": {"linux": {"image": params.node_image}},
            "nodes": nodes,
            "links": links,
        },
    }


def _iface_index(iface: str) -> int:
    if iface.startswith("eth"):
        suffix = iface[3:]
        if suffix.isdigit():
            return int(suffix)
    return 1 << 30


def _edge_ip_pair(edge_idx: int) -> Tuple[str, str]:
    second_octet = edge_idx // 256
    third_octet = edge_idx % 256
    if second_octet > 255:
        raise ValueError("Too many edges for /30 IP allocation plan (max 65536 links).")
    subnet = f"10.{second_octet}.{third_octet}"
    return f"{subnet}.1", f"{subnet}.2"
