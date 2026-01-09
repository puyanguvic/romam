#!/usr/bin/env python3

import argparse
import ipaddress
import json
import os
import tempfile
import time
try:
    import tomllib  # py3.11+
except Exception:  # pragma: no cover
    tomllib = None

from mininet.link import TCLink
from mininet.log import setLogLevel, info
from mininet.net import Mininet


def load_spec(path: str):
    with open(path, "rb") as f:
        raw = f.read()
    if path.endswith(".toml"):
        if tomllib is None:
            raise ValueError("TOML spec requires Python 3.11+ (or use .json)")
        return tomllib.loads(raw.decode("utf-8"))
    if path.endswith(".json"):
        return json.loads(raw.decode("utf-8"))
    raise ValueError("spec must be .toml or .json")


def require(obj, key, typ):
    if key not in obj:
        raise ValueError(f"missing required field: {key}")
    if not isinstance(obj[key], typ):
        raise ValueError(f"field '{key}' must be {typ.__name__}")
    return obj[key]


def optional(obj, key, typ, default=None):
    if key not in obj:
        return default
    if not isinstance(obj[key], typ):
        raise ValueError(f"field '{key}' must be {typ.__name__}")
    return obj[key]


def parse_multicast(s: str):
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


def link_subnet_allocator(link_base: str, prefixlen: int):
    net = ipaddress.IPv4Network(link_base, strict=True)
    if prefixlen < net.prefixlen:
        raise ValueError("link_prefixlen must be >= link_base prefixlen")
    subnet_size = 1 << (32 - prefixlen)
    cursor = int(net.network_address)
    end = int(net.broadcast_address) + 1
    while cursor + subnet_size <= end:
        yield ipaddress.IPv4Network((cursor, prefixlen))
        cursor += subnet_size


def validate_and_normalize(spec: dict):
    version = require(spec, "version", int)
    if version != 1:
        raise ValueError("unsupported spec version")

    romam = spec.get("romam", {})
    if romam is None:
        romam = {}
    if not isinstance(romam, dict):
        raise ValueError("romam must be a table/object")

    multicast = optional(romam, "multicast", str, "239.255.0.1:5000")
    _ = parse_multicast(multicast)

    addressing = spec.get("addressing", {})
    if addressing is None:
        addressing = {}
    if not isinstance(addressing, dict):
        raise ValueError("addressing must be a table/object")
    loopback_base = optional(addressing, "loopback_base", str, "10.255.0.0/16")
    link_base = optional(addressing, "link_base", str, "10.0.0.0/8")
    link_prefixlen = int(optional(addressing, "link_prefixlen", int, 30))
    ipaddress.IPv4Network(loopback_base, strict=True)
    ipaddress.IPv4Network(link_base, strict=True)
    if link_prefixlen < 16 or link_prefixlen > 30:
        raise ValueError("addressing.link_prefixlen must be 16..30")

    defaults = spec.get("defaults", {})
    if defaults is None:
        defaults = {}
    if not isinstance(defaults, dict):
        raise ValueError("defaults must be a table/object")
    default_cost = int(optional(defaults, "cost", int, 1))
    if default_cost < 1:
        raise ValueError("defaults.cost must be >= 1")
    default_tc = defaults.get("tc", {}) or {}
    if not isinstance(default_tc, dict):
        raise ValueError("defaults.tc must be a table/object")

    nodes = require(spec, "nodes", list)
    links = require(spec, "links", list)

    node_by_name = {}
    for n in nodes:
        if not isinstance(n, dict):
            raise ValueError("each node must be a table/object")
        name = require(n, "name", str)
        if name in node_by_name:
            raise ValueError(f"duplicate node name: {name}")
        node_id = int(require(n, "id", int))
        if node_id < 0:
            raise ValueError(f"node id must be >=0: {name}")
        router_id = optional(n, "router_id", str, None)
        if router_id is None:
            router_id = compute_router_id(loopback_base, node_id)
        else:
            ipaddress.IPv4Address(router_id)
        advertise = optional(n, "advertise", list, [])
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
        a = require(e, "a", str)
        b = require(e, "b", str)
        if a not in node_by_name or b not in node_by_name:
            raise ValueError(f"link endpoint not found: {a} {b}")
        if a == b:
            raise ValueError("self-loop not allowed")
        key = tuple(sorted([a, b]))
        if key in seen:
            raise ValueError(f"duplicate link: {key[0]} {key[1]}")
        seen.add(key)
        cost = int(optional(e, "cost", int, default_cost))
        if cost < 1:
            raise ValueError(f"link cost must be >=1: {a}-{b}")
        tc = e.get("tc", {}) or {}
        if not isinstance(tc, dict):
            raise ValueError("link.tc must be a table/object")
        link_tc = dict(default_tc)
        link_tc.update(tc)
        norm_links.append({"a": a, "b": b, "cost": cost, "tc": link_tc})

    return {
        "name": optional(spec, "name", str, os.path.basename(spec.get("name", "spec"))),
        "romam": {
            "multicast": multicast,
            "hello_interval_ms": int(optional(romam, "hello_interval_ms", int, 500)),
            "dead_interval_ms": int(optional(romam, "dead_interval_ms", int, 2000)),
            "lsa_interval_ms": int(optional(romam, "lsa_interval_ms", int, 1000)),
            "route_table": int(optional(romam, "route_table", int, 100)),
            "route_metric_base": int(optional(romam, "route_metric_base", int, 100)),
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True, help="experiment spec (.toml or .json)")
    ap.add_argument("--romamd", default="./build-standalone/daemon/romamd")
    ap.add_argument("--out", default=None, help="output directory for configs/logs (default: temp)")
    ap.add_argument("--dry-run", action="store_true", help="run romamd without programming routes")
    ap.add_argument("--no-ping", action="store_true", help="skip reachability self-check ping")
    args = ap.parse_args()

    if not os.path.exists(args.romamd):
        ap.error(f"romamd not found: {args.romamd}")

    spec = validate_and_normalize(load_spec(args.spec))
    route_table = spec["romam"]["route_table"]
    if route_table < 0 or route_table > 255:
        ap.error("romam.route_table must be 0-255")

    setLogLevel("info")
    net = Mininet(controller=None, link=TCLink, autoSetMacs=True, autoStaticArp=True)

    routers = {}
    for n in spec["nodes"]:
        name = n["name"]
        routers[name] = net.addHost(name)

    for e in spec["links"]:
        link_params = {}
        tc = e.get("tc", {}) or {}
        if "delay" in tc:
            link_params["delay"] = tc["delay"]
        if "loss" in tc:
            link_params["loss"] = float(tc["loss"])
        if "bw" in tc:
            link_params["bw"] = float(tc["bw"])
        net.addLink(routers[e["a"]], routers[e["b"]], **link_params)

    net.start()

    tmpdir = args.out or tempfile.mkdtemp(prefix="romam-mn-")
    os.makedirs(tmpdir, exist_ok=True)
    info(f"*** configs/logs: {tmpdir}\n")

    # enable forwarding, set up a dedicated route table rule, assign loopback router_id
    for n in spec["nodes"]:
        name = n["name"]
        h = routers[name]
        rid = n["router_id"]
        h.cmd("sysctl -w net.ipv4.ip_forward=1 >/dev/null 2>&1")
        h.cmd(f"ip -4 addr add {rid}/32 dev lo 2>/dev/null || true")
        h.cmd(f"ip -4 route flush table {route_table} 2>/dev/null || true")
        h.cmd(f"ip -4 rule del pref 1000 lookup {route_table} 2>/dev/null || true")
        h.cmd(f"ip -4 rule add pref 1000 lookup {route_table} 2>/dev/null || true")

    # assign per-link addressing on both ends (deterministic by spec order)
    alloc = link_subnet_allocator(spec["addressing"]["link_base"], spec["addressing"]["link_prefixlen"])
    for e in spec["links"]:
        a = e["a"]
        b = e["b"]
        ha = routers[a]
        hb = routers[b]
        subnet = next(alloc)
        hosts = list(subnet.hosts())
        if len(hosts) < 2:
            raise ValueError("link subnet too small")
        ip_a = f"{hosts[0]}/{subnet.prefixlen}"
        ip_b = f"{hosts[1]}/{subnet.prefixlen}"
        intf_a = ha.connectionsTo(hb)[0][0].name
        intf_b = hb.connectionsTo(ha)[0][0].name
        ha.cmd(f"ip -4 addr add {ip_a} dev {intf_a}")
        hb.cmd(f"ip -4 addr add {ip_b} dev {intf_b}")

    # start romamd on each router
    for n in spec["nodes"]:
        name = n["name"]
        h = routers[name]
        rid = n["router_id"]
        conf_path = os.path.join(tmpdir, f"{name}.conf")
        log_path = os.path.join(tmpdir, f"{name}.log")
        pid_path = os.path.join(tmpdir, f"{name}.pid")

        with open(conf_path, "w", encoding="utf-8") as f:
            f.write(f"router_id={rid}\n")
            f.write(f"loopback={rid}/32\n")
            f.write(f"multicast={spec['romam']['multicast']}\n")
            f.write(f"hello_interval_ms={spec['romam']['hello_interval_ms']}\n")
            f.write(f"dead_interval_ms={spec['romam']['dead_interval_ms']}\n")
            f.write(f"lsa_interval_ms={spec['romam']['lsa_interval_ms']}\n")
            f.write(f"route_table={spec['romam']['route_table']}\n")
            f.write(f"route_metric_base={spec['romam']['route_metric_base']}\n")

            intfs = [ifname for ifname in h.intfNames() if ifname != "lo"]
            for ifname in intfs:
                f.write(f"iface={ifname}\n")

            # per-interface cost from spec links
            for e in spec["links"]:
                if name != e["a"] and name != e["b"]:
                    continue
                peer = e["b"] if name == e["a"] else e["a"]
                intf_local = h.connectionsTo(routers[peer])[0][0].name
                f.write(f"iface_cost={intf_local}:{e['cost']}\n")

            for pfx in n.get("advertise", []):
                f.write(f"prefix={pfx}\n")

        dry = "--dry-run" if args.dry_run else ""
        h.cmd(f"nohup {args.romamd} --config {conf_path} {dry} >{log_path} 2>&1 & echo $! > {pid_path}")

    info("*** waiting for convergence...\n")
    time.sleep(3)

    # quick reachability check: ping all router loopbacks from r1
    if spec["nodes"] and (not args.no_ping):
        src_name = spec["nodes"][0]["name"]
        src = routers[src_name]
        for n in spec["nodes"][1:]:
            rid = n["router_id"]
            out = src.cmd(f"ping -c 1 -W 1 {rid} | tail -n 2")
            info(f"*** ping {src_name} -> {n['name']} ({rid}):\n{out}\n")

    info("*** done. press Ctrl-C to stop.\n")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        info("*** stopping daemons...\n")
        for n in spec["nodes"]:
            name = n["name"]
            h = routers[name]
            pid_path = os.path.join(tmpdir, f"{name}.pid")
            h.cmd(f"test -f {pid_path} && kill $(cat {pid_path}) 2>/dev/null || true")
        net.stop()


if __name__ == "__main__":
    main()
