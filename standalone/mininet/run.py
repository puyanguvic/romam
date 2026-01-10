#!/usr/bin/env python3

import argparse
import os
import sys
import tempfile
import time

from mininet.link import TCLink
from mininet.log import info, setLogLevel
from mininet.net import Mininet

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.spec import link_subnet_allocator, load_spec, validate_and_normalize  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True, help="experiment spec (.toml or .json)")
    ap.add_argument("--romamd", default="./build-standalone/daemon/romamd")
    ap.add_argument("--out", default=None, help="output directory for configs/logs (default: temp)")
    ap.add_argument("--dry-run", action="store_true", help="run romamd without programming routes")
    ap.add_argument("--no-ping", action="store_true", help="skip reachability self-check ping")
    ap.add_argument("--duration", type=int, default=0, help="auto-stop after N seconds (0 = wait Ctrl-C)")
    ap.add_argument("--cli", action="store_true", help="start Mininet CLI after convergence")
    ap.add_argument("--show-routes", action="store_true", help="print routes in romam route_table after convergence")
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
            f.write(f"routing_algo={spec['romam']['routing_algo']}\n")
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

    if args.show_routes:
        for n in spec["nodes"]:
            name = n["name"]
            h = routers[name]
            out = h.cmd(f"ip -4 route show table {route_table} | sed -n '1,200p'")
            info(f"*** routes {name} table {route_table}:\n{out}\n")

    # quick reachability check: ping all router loopbacks from r1
    if spec["nodes"] and (not args.no_ping):
        src_name = spec["nodes"][0]["name"]
        src = routers[src_name]
        for n in spec["nodes"][1:]:
            rid = n["router_id"]
            out = src.cmd(f"ping -c 1 -W 1 {rid} | tail -n 2")
            info(f"*** ping {src_name} -> {n['name']} ({rid}):\n{out}\n")

    try:
        if args.cli:
            info("*** entering Mininet CLI (exit with Ctrl-D or 'exit')...\n")
            from mininet.cli import CLI  # local import to keep dependencies minimal

            CLI(net)
            return
        if args.duration > 0:
            info(f"*** running for {args.duration}s...\n")
            time.sleep(args.duration)
            return
        info("*** done. press Ctrl-C to stop.\n")
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
