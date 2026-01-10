from __future__ import annotations

import argparse
import os
import stat
from typing import Dict, List, Tuple

from romam_lab.topology.spec import link_subnet_allocator, load_spec, validate_and_normalize


def _yq(s: str) -> str:
    if s == "":
        return "''"
    safe = True
    for ch in s:
        if not (ch.isalnum() or ch in "._-/"):
            safe = False
            break
    if safe:
        return s
    return "'" + s.replace("'", "''") + "'"


def _write_executable(path: str, content: str):
    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    st = os.stat(path)
    os.chmod(path, st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def generate(*, spec_path: str, out_dir: str, name: str | None = None, image: str = "romam:clab", route_table_pref: int = 1000):
    spec = validate_and_normalize(load_spec(spec_path))
    lab_name = name or f"romam-{spec['name']}"

    out_dir = os.path.abspath(out_dir)
    topo_dir = out_dir
    cfg_dir = os.path.join(out_dir, "configs")
    os.makedirs(topo_dir, exist_ok=True)
    os.makedirs(cfg_dir, exist_ok=True)

    route_table = int(spec["romam"]["route_table"])
    route_pref = str(int(route_table_pref))

    next_if = {n["name"]: 1 for n in spec["nodes"]}  # eth0 is usually mgmt
    if_for_peer: Dict[Tuple[str, str], str] = {}
    link_if: List[Dict] = []
    for e in spec["links"]:
        a, b = e["a"], e["b"]
        ia = f"eth{next_if[a]}"
        ib = f"eth{next_if[b]}"
        next_if[a] += 1
        next_if[b] += 1
        if_for_peer[(a, b)] = ia
        if_for_peer[(b, a)] = ib
        link_if.append({**e, "if_a": ia, "if_b": ib})

    alloc = link_subnet_allocator(spec["addressing"]["link_base"], spec["addressing"]["link_prefixlen"])
    for e in link_if:
        subnet = next(alloc)
        hosts = list(subnet.hosts())
        if len(hosts) < 2:
            raise ValueError("link subnet too small")
        e["ip_a"] = f"{hosts[0]}/{subnet.prefixlen}"
        e["ip_b"] = f"{hosts[1]}/{subnet.prefixlen}"
        e["pfxlen"] = subnet.prefixlen

    for node in spec["nodes"]:
        name = node["name"]
        rid = node["router_id"]
        conf_path = os.path.join(cfg_dir, f"{name}.conf")
        start_path = os.path.join(cfg_dir, f"{name}.start.sh")

        ifaces = []
        for e in link_if:
            if e["a"] == name:
                ifaces.append(e["if_a"])
            elif e["b"] == name:
                ifaces.append(e["if_b"])

        with open(conf_path, "w", encoding="utf-8") as f:
            f.write(f"router_id={rid}\n")
            f.write(f"loopback={rid}/32\n")
            f.write(f"multicast={spec['romam']['multicast']}\n")
            f.write(f"routing_algo={spec['romam']['routing_algo']}\n")
            f.write(f"hello_interval_ms={spec['romam']['hello_interval_ms']}\n")
            f.write(f"dead_interval_ms={spec['romam']['dead_interval_ms']}\n")
            f.write(f"lsa_interval_ms={spec['romam']['lsa_interval_ms']}\n")
            f.write(f"route_table={spec['romam']['route_table']}\n")
            f.write(f"route_metric_base={spec['romam']['route_metric_base']}\n")
            for ifname in ifaces:
                f.write(f"iface={ifname}\n")
            for e in link_if:
                if e["a"] == name:
                    f.write(f"iface_cost={e['if_a']}:{e['cost']}\n")
                elif e["b"] == name:
                    f.write(f"iface_cost={e['if_b']}:{e['cost']}\n")
            for pfx in node.get("advertise", []):
                f.write(f"prefix={pfx}\n")

        lines = [
            "#!/usr/bin/env bash",
            "set -euo pipefail",
            "",
            "ROMAMD_BIN=${ROMAMD_BIN:-romamd}",
            "ROMAMD_ARGS=${ROMAMD_ARGS:-}",
            "",
            "sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1 || true",
            "sysctl -w net.ipv4.conf.all.rp_filter=0 >/dev/null 2>&1 || true",
            "sysctl -w net.ipv4.conf.default.rp_filter=0 >/dev/null 2>&1 || true",
            "",
            f"ip -4 addr add {rid}/32 dev lo 2>/dev/null || true",
            f"ip -4 route flush table {route_table} 2>/dev/null || true",
            f"ip -4 rule del pref {route_pref} lookup {route_table} 2>/dev/null || true",
            f"ip -4 rule add pref {route_pref} lookup {route_table}",
            "",
        ]
        for e in link_if:
            if e["a"] == name:
                lines.append(f"ip -4 addr add {e['ip_a']} dev {e['if_a']} 2>/dev/null || true")
                lines.append(f"ip link set {e['if_a']} up")
                tc = e.get("tc", {}) or {}
                if any(k in tc for k in ("bw", "delay", "loss")):
                    cmd = f"tc qdisc replace dev {e['if_a']} root netem"
                    if "bw" in tc:
                        cmd += f" rate {float(tc['bw'])}mbit"
                    if "delay" in tc:
                        cmd += f" delay {str(tc['delay'])}"
                    if "loss" in tc:
                        cmd += f" loss {float(tc['loss'])}%"
                    lines.append(cmd)
            elif e["b"] == name:
                lines.append(f"ip -4 addr add {e['ip_b']} dev {e['if_b']} 2>/dev/null || true")
                lines.append(f"ip link set {e['if_b']} up")
                tc = e.get("tc", {}) or {}
                if any(k in tc for k in ("bw", "delay", "loss")):
                    cmd = f"tc qdisc replace dev {e['if_b']} root netem"
                    if "bw" in tc:
                        cmd += f" rate {float(tc['bw'])}mbit"
                    if "delay" in tc:
                        cmd += f" delay {str(tc['delay'])}"
                    if "loss" in tc:
                        cmd += f" loss {float(tc['loss'])}%"
                    lines.append(cmd)
        lines += [
            "",
            "exec ${ROMAMD_BIN} --config /etc/romam/romamd.conf ${ROMAMD_ARGS}",
            "",
        ]
        _write_executable(start_path, "\n".join(lines))

    topo_path = os.path.join(topo_dir, "topo.clab.yml")
    rel_cfg_dir = "./configs"

    y: List[str] = []
    y.append(f"name: {_yq(lab_name)}")
    y.append("topology:")
    y.append("  kinds:")
    y.append("    linux:")
    y.append(f"      image: {_yq(image)}")
    y.append("  nodes:")
    for node in spec["nodes"]:
        name = node["name"]
        y.append(f"    {_yq(name)}:")
        y.append("      kind: linux")
        y.append("      cap-add:")
        y.append("        - NET_ADMIN")
        y.append("      binds:")
        y.append(f"        - {_yq(os.path.join(rel_cfg_dir, f'{name}.conf'))}:/etc/romam/romamd.conf:ro")
        y.append(f"        - {_yq(os.path.join(rel_cfg_dir, f'{name}.start.sh'))}:/etc/romam/start.sh:ro")
        y.append("      env:")
        y.append("        ROMAMD_BIN: romamd")
    y.append("  links:")
    for e in link_if:
        y.append("    - endpoints:")
        y.append(f"        - {_yq(e['a'] + ':' + e['if_a'])}")
        y.append(f"        - {_yq(e['b'] + ':' + e['if_b'])}")

    with open(topo_path, "w", encoding="utf-8") as f:
        f.write("\n".join(y) + "\n")

    return {
        "topo_path": topo_path,
        "cfg_dir": cfg_dir,
        "lab_name": lab_name,
        "image": image,
    }


def main(argv: List[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True, help="spec (.toml or .json), same as standalone/mininet/specs")
    ap.add_argument("--out", required=True, help="output directory to write topo + configs")
    ap.add_argument("--name", default=None, help="containerlab lab name (default: from spec.name)")
    ap.add_argument("--image", default="romam:clab", help="docker image for linux nodes")
    ap.add_argument("--route-table-pref", default="1000", help="ip rule pref for route_table lookup")
    args = ap.parse_args(argv)

    res = generate(
        spec_path=args.spec,
        out_dir=args.out,
        name=args.name,
        image=args.image,
        route_table_pref=int(args.route_table_pref),
    )

    print(f"wrote: {res['topo_path']}")
    print(f"wrote: {res['cfg_dir']}/<node>.conf and {res['cfg_dir']}/<node>.start.sh")
    print("")
    print("next:")
    print(f"  docker build -f standalone/docker/Dockerfile.clab -t {res['image']} .")
    print(f"  containerlab deploy -t {res['topo_path']}")
    print(f"  containerlab destroy -t {res['topo_path']}")
    return 0
