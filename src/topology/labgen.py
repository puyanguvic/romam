from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import yaml

from irp.utils.io import ensure_dir
from topology.clab_loader import load_clab_topology


@dataclass(frozen=True)
class LabGenParams:
    protocol: str
    topology_file: Path
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
    mgmt_network_name: str
    mgmt_ipv4_subnet: str
    mgmt_ipv6_subnet: str
    mgmt_external_access: bool


def generate_routerd_lab(params: LabGenParams) -> Dict[str, str]:
    try:
        source = load_clab_topology(params.topology_file)
        output_dir = ensure_dir(params.output_dir)
        configs_dir = ensure_dir(output_dir / "configs")

        node_names = list(source.node_names)
        rid_map = {node_name: idx + 1 for idx, node_name in enumerate(node_names)}
        iface_plans: Dict[str, List[Dict[str, Any]]] = {node_name: [] for node_name in node_names}

        for link in source.links:
            left_rid = rid_map[link.left_node]
            right_rid = rid_map[link.right_node]
            iface_plans[link.left_node].append(
                {
                    "iface": link.left_iface,
                    "local_ip": link.left_ip,
                    "neighbor_ip": link.right_ip,
                    "neighbor_id": right_rid,
                    "cost": float(link.cost),
                }
            )
            iface_plans[link.right_node].append(
                {
                    "iface": link.right_iface,
                    "local_ip": link.right_ip,
                    "neighbor_ip": link.left_ip,
                    "neighbor_id": left_rid,
                    "cost": float(link.cost),
                }
            )

        for node_name in node_names:
            rid = rid_map[node_name]
            cfg = _build_routerd_config(params, rid, iface_plans[node_name])
            config_path = configs_dir / f"{node_name}.yaml"
            with config_path.open("w", encoding="utf-8") as f:
                yaml.safe_dump(cfg, f, sort_keys=False, width=4096)

        deploy_env_file = _write_deploy_env(params, output_dir)

        return {
            "topology_file": str(source.source_path),
            "configs_dir": str(configs_dir),
            "lab_name": params.lab_name,
            "source_topology_file": str(source.source_path),
            "deploy_env_file": str(deploy_env_file),
        }
    except PermissionError as exc:
        output_base = params.output_dir
        raise PermissionError(
            f"Permission denied while writing lab assets under '{output_base}'. "
            "This is usually caused by previous root-owned files. "
            "Run: sudo chown -R $USER:$USER results/runs/routerd_labs"
        ) from exc


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


def _write_deploy_env(params: LabGenParams, output_dir: Path) -> Path:
    env_path = output_dir / "deploy.env"
    entries = {
        "CLAB_LAB_NAME": params.lab_name,
        "CLAB_NODE_IMAGE": params.node_image,
        "CLAB_MGMT_NETWORK": params.mgmt_network_name,
        "CLAB_MGMT_IPV4_SUBNET": params.mgmt_ipv4_subnet,
        "CLAB_MGMT_IPV6_SUBNET": params.mgmt_ipv6_subnet,
        "CLAB_MGMT_EXTERNAL_ACCESS": "true" if params.mgmt_external_access else "false",
        "ROMAM_LOG_LEVEL": params.log_level,
    }
    with env_path.open("w", encoding="utf-8") as f:
        for key, value in entries.items():
            f.write(f"{key}={value}\n")
    return env_path
