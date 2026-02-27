from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List

import yaml

from clab.clab_loader import load_clab_topology
from irp.utils.io import ensure_dir

SUPPORTED_LAB_PROTOCOLS: tuple[str, ...] = (
    "ospf",
    "rip",
    "ecmp",
    "topk",
    "ddr",
    "dgr",
    "octopus",
)


@dataclass(frozen=True)
class LabGenParams:
    protocol: str
    routing_alpha: float
    routing_beta: float
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
    forwarding_enabled: bool = False
    forwarding_dry_run: bool = True
    mgmt_http_enabled: bool = True
    mgmt_http_bind: str = "0.0.0.0"
    mgmt_http_port_base: int = 18000
    mgmt_grpc_enabled: bool = True
    mgmt_grpc_bind: str = "0.0.0.0"
    mgmt_grpc_port_base: int = 19000
    ddr_k_paths: int = 3
    ddr_deadline_ms: float = 100.0
    ddr_flow_size_bytes: float = 64000.0
    ddr_link_bandwidth_bps: float = 9600000.0
    ddr_queue_sample_interval: float = 1.0
    ddr_queue_levels: int = 4
    ddr_pressure_threshold: int = 2
    ddr_queue_level_scale_ms: float = 8.0
    ddr_randomize_route_selection: bool = False
    ddr_rng_seed: int = 1
    ecmp_hash_seed: int = 1
    topk_k_paths: int = 3
    topk_explore_probability: float = 0.3
    topk_selection_hold_time_s: float = 3.0
    topk_rng_seed: int = 1


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

        router_host_ips = _select_router_host_ips(node_names, iface_plans)

        for node_name in node_names:
            rid = rid_map[node_name]
            cfg = _build_routerd_config(
                params=params,
                router_id=rid,
                local_ifaces=iface_plans[node_name],
                all_node_names=node_names,
                rid_map=rid_map,
                router_host_ips=router_host_ips,
            )
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
    all_node_names: List[str],
    rid_map: Dict[str, int],
    router_host_ips: Dict[str, str],
) -> Dict[str, Any]:
    neighbors = [
        {
            "router_id": int(item["neighbor_id"]),
            "address": str(item["neighbor_ip"]),
            "port": int(params.bind_port),
            "cost": float(item["cost"]),
            "iface": str(item["iface"]),
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
        "forwarding": _build_forwarding_cfg(
            params=params,
            local_ifaces=local_ifaces,
            all_node_names=all_node_names,
            rid_map=rid_map,
            router_host_ips=router_host_ips,
        ),
        "management": {
            "http": {
                "enabled": bool(params.mgmt_http_enabled),
                "bind": str(params.mgmt_http_bind),
                "port": int(params.mgmt_http_port_base) + int(router_id),
            },
            "grpc": {
                "enabled": bool(params.mgmt_grpc_enabled),
                "bind": str(params.mgmt_grpc_bind),
                "port": int(params.mgmt_grpc_port_base) + int(router_id),
            },
        },
    }
    if params.protocol == "ospf":
        cfg["protocol_params"] = {
            "ospf": {
                "hello_interval": float(params.ospf_hello_interval),
                "lsa_interval": float(params.ospf_lsa_interval),
                "lsa_max_age": float(params.ospf_lsa_max_age),
            }
        }
    elif params.protocol == "rip":
        cfg["protocol_params"] = {
            "rip": {
                "update_interval": float(params.rip_update_interval),
                "neighbor_timeout": float(params.rip_neighbor_timeout),
                "infinity_metric": float(params.rip_infinity_metric),
                "poison_reverse": bool(params.rip_poison_reverse),
            }
        }
    elif params.protocol == "ecmp":
        cfg["protocol_params"] = {
            "ecmp": {
                "hello_interval": float(params.ospf_hello_interval),
                "lsa_interval": float(params.ospf_lsa_interval),
                "lsa_max_age": float(params.ospf_lsa_max_age),
                "hash_seed": int(params.ecmp_hash_seed),
            }
        }
    elif params.protocol == "topk":
        cfg["protocol_params"] = {
            "topk": {
                "hello_interval": float(params.ospf_hello_interval),
                "lsa_interval": float(params.ospf_lsa_interval),
                "lsa_max_age": float(params.ospf_lsa_max_age),
                "k_paths": int(params.topk_k_paths),
                "explore_probability": float(params.topk_explore_probability),
                "selection_hold_time_s": float(params.topk_selection_hold_time_s),
                "rng_seed": int(params.topk_rng_seed),
            }
        }
    elif params.protocol in {"ddr", "dgr", "octopus"}:
        cfg["protocol_params"] = {
            params.protocol: {
                "hello_interval": float(params.ospf_hello_interval),
                "lsa_interval": float(params.ospf_lsa_interval),
                "lsa_max_age": float(params.ospf_lsa_max_age),
                "queue_sample_interval": float(params.ddr_queue_sample_interval),
                "k_paths": int(params.ddr_k_paths),
                "deadline_ms": float(params.ddr_deadline_ms),
                "flow_size_bytes": float(params.ddr_flow_size_bytes),
                "link_bandwidth_bps": float(params.ddr_link_bandwidth_bps),
                "queue_levels": int(params.ddr_queue_levels),
                "pressure_threshold": int(params.ddr_pressure_threshold),
                "queue_level_scale_ms": float(params.ddr_queue_level_scale_ms),
                "randomize_route_selection": bool(params.ddr_randomize_route_selection),
                "rng_seed": int(params.ddr_rng_seed),
            }
        }
    else:
        raise ValueError(
            f"Unsupported protocol '{params.protocol}'. Supported protocols: "
            + ", ".join(SUPPORTED_LAB_PROTOCOLS)
        )
    return cfg


def _build_forwarding_cfg(
    params: LabGenParams,
    local_ifaces: List[Dict[str, Any]],
    all_node_names: List[str],
    rid_map: Dict[str, int],
    router_host_ips: Dict[str, str],
) -> Dict[str, Any]:
    forwarding: Dict[str, Any] = {
        "enabled": bool(params.forwarding_enabled),
        "dry_run": bool(params.forwarding_dry_run),
    }
    if not params.forwarding_enabled:
        return forwarding

    destination_prefixes: Dict[int, str] = {}
    for node_name in sorted(all_node_names, key=lambda name: int(rid_map[name])):
        rid = int(rid_map[node_name])
        destination_prefixes[rid] = f"{router_host_ips[node_name]}/32"

    next_hop_ips: Dict[int, str] = {}
    for item in sorted(local_ifaces, key=lambda x: int(x["neighbor_id"])):
        next_hop_ips[int(item["neighbor_id"])] = str(item["neighbor_ip"])

    forwarding["destination_prefixes"] = destination_prefixes
    forwarding["next_hop_ips"] = next_hop_ips
    return forwarding


def _select_router_host_ips(
    node_names: List[str],
    iface_plans: Dict[str, List[Dict[str, Any]]],
) -> Dict[str, str]:
    out: Dict[str, str] = {}
    for node_name in node_names:
        local_ifaces = list(iface_plans.get(node_name, []))
        if not local_ifaces:
            raise ValueError(
                f"Node '{node_name}' has no interfaces; cannot build forwarding prefixes."
            )
        first = sorted(local_ifaces, key=lambda x: str(x["iface"]))[0]
        out[node_name] = str(first["local_ip"])
    return out


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
