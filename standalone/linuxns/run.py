#!/usr/bin/env python3

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../.."))
sys.path.insert(0, REPO_ROOT)

from romam_lab.topology.spec import link_subnet_allocator, load_spec, validate_and_normalize  # noqa: E402


def sh(args, *, check=True, capture=False, text=True):
    if isinstance(args, str):
        raise TypeError("sh() expects argv list, not a string")
    res = subprocess.run(args, check=False, capture_output=capture, text=text)
    if check and res.returncode != 0:
        msg = f"command failed ({res.returncode}): {' '.join(args)}"
        if capture and res.stderr:
            msg += f"\n{res.stderr.strip()}"
        raise RuntimeError(msg)
    return res


def netns_exec(ns: str, *args: str, **kw):
    argv = ["ip", "netns", "exec", ns] + list(args)
    return sh(argv, **kw)


def set_netns_sysctl(ns: str, key: str, value: str):
    key = key.strip()
    value = value.strip()
    if not key or not value:
        raise ValueError("sysctl key/value must be non-empty")

    # Prefer sysctl if available; otherwise write to /proc/sys.
    if shutil.which("sysctl") is not None:
        netns_exec(ns, "sysctl", "-w", f"{key}={value}", check=False, capture=True)
        return

    path = "/proc/sys/" + key.replace(".", "/")
    netns_exec(ns, "sh", "-lc", f"echo {value} > {path}", check=False, capture=True)


def tune_netns(ns: str):
    # Forwarding is required for multi-hop reachability.
    set_netns_sysctl(ns, "net.ipv4.ip_forward", "1")
    # rp_filter may drop forwarded packets in synthetic topologies.
    set_netns_sysctl(ns, "net.ipv4.conf.all.rp_filter", "0")
    set_netns_sysctl(ns, "net.ipv4.conf.default.rp_filter", "0")
    set_netns_sysctl(ns, "net.ipv4.conf.all.send_redirects", "0")


def tune_netns_iface(ns: str, ifname: str):
    set_netns_sysctl(ns, f"net.ipv4.conf.{ifname}.rp_filter", "0")


def ip(*args, ns=None, **kw):
    argv = ["ip"]
    if ns is not None:
        argv += ["-n", ns]
    argv += list(args)
    return sh(argv, **kw)


def tc(*args, ns=None, **kw):
    argv = ["tc"]
    if ns is not None:
        argv = ["ip", "netns", "exec", ns, "tc"]
    argv += list(args)
    return sh(argv, **kw)


def list_netns():
    res = sh(["ip", "netns", "list"], check=False, capture=True)
    if res.returncode != 0:
        return []
    out = (res.stdout or "").strip()
    if not out:
        return []
    names = []
    for line in out.splitlines():
        name = line.split()[0]
        names.append(name)
    return names


def apply_tc(ns: str, ifname: str, tc_spec: dict):
    delay = tc_spec.get("delay")
    loss = tc_spec.get("loss")
    bw = tc_spec.get("bw")
    if delay is None and loss is None and bw is None:
        return

    cmd = ["qdisc", "replace", "dev", ifname, "root", "netem"]
    if bw is not None:
        cmd += ["rate", f"{float(bw)}mbit"]
    if delay is not None:
        cmd += ["delay", str(delay)]
    if loss is not None:
        cmd += ["loss", f"{float(loss)}%"]
    tc(*cmd, ns=ns)


def ensure_tools():
    for tool in ("ip", "tc"):
        if shutil.which(tool) is None:
            raise RuntimeError(f"missing required tool: {tool}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--spec", required=True, help="experiment spec (.toml or .json)")
    ap.add_argument("--romamd", default="./build-standalone/daemon/romamd")
    ap.add_argument("--out", default=None, help="output directory for configs/logs (default: temp)")
    ap.add_argument("--dry-run", action="store_true", help="run romamd without programming routes")
    ap.add_argument("--no-ping", action="store_true", help="skip reachability self-check ping")
    ap.add_argument("--duration", type=int, default=0, help="auto-stop after N seconds (0 = wait Ctrl-C)")
    ap.add_argument("--show-routes", action="store_true", help="print routes in romam route_table after convergence")
    ap.add_argument("--prefix", default="romam-", help="netns name prefix (default: romam-)")
    ap.add_argument("--cleanup", action="store_true", help="delete existing netns with the same prefix before run")
    args = ap.parse_args()

    ensure_tools()
    if os.geteuid() != 0:
        ap.error("this runner needs root (ip netns/tc). try: sudo python3 standalone/linuxns/run.py ...")

    romamd = os.path.abspath(args.romamd)
    if not os.path.exists(romamd):
        ap.error(f"romamd not found: {romamd}")

    spec = validate_and_normalize(load_spec(args.spec))
    route_table = spec["romam"]["route_table"]
    if route_table < 0 or route_table > 255:
        ap.error("romam.route_table must be 0-255")

    tmpdir = args.out or tempfile.mkdtemp(prefix="romam-ns-")
    os.makedirs(tmpdir, exist_ok=True)
    print(f"*** configs/logs: {tmpdir}")

    ns_names = {n["name"]: f"{args.prefix}{n['name']}" for n in spec["nodes"]}
    existing = set(list_netns())
    if args.cleanup:
        for ns in sorted(existing):
            if ns.startswith(args.prefix):
                sh(["ip", "netns", "del", ns], check=False)
        existing = set(list_netns())

    for ns in ns_names.values():
        if ns in existing:
            ap.error(f"netns already exists: {ns} (use --cleanup)")

    # create namespaces
    for ns in ns_names.values():
        sh(["ip", "netns", "add", ns])
        ip("link", "set", "lo", "up", ns=ns)
        tune_netns(ns)

    # assign loopback router_id, route table rule
    for n in spec["nodes"]:
        ns = ns_names[n["name"]]
        rid = n["router_id"]
        ip("addr", "add", f"{rid}/32", "dev", "lo", ns=ns, check=False)
        ip("route", "flush", "table", str(route_table), ns=ns, check=False, capture=True)
        ip("rule", "del", "pref", "1000", "lookup", str(route_table), ns=ns, check=False, capture=True)
        ip("rule", "add", "pref", "1000", "lookup", str(route_table), ns=ns, check=True)

    # create links: veth pairs moved into namespaces, renamed per node as ethX
    next_if_index = {n["name"]: 0 for n in spec["nodes"]}
    iface_for_link = {}
    alloc = link_subnet_allocator(spec["addressing"]["link_base"], spec["addressing"]["link_prefixlen"])

    for idx, e in enumerate(spec["links"]):
        a = e["a"]
        b = e["b"]
        ns_a = ns_names[a]
        ns_b = ns_names[b]

        va = f"v{idx}a"
        vb = f"v{idx}b"
        sh(["ip", "link", "add", va, "type", "veth", "peer", "name", vb])
        sh(["ip", "link", "set", va, "netns", ns_a])
        sh(["ip", "link", "set", vb, "netns", ns_b])

        if_a = f"eth{next_if_index[a]}"
        if_b = f"eth{next_if_index[b]}"
        next_if_index[a] += 1
        next_if_index[b] += 1

        ip("link", "set", va, "name", if_a, ns=ns_a)
        ip("link", "set", vb, "name", if_b, ns=ns_b)

        subnet = next(alloc)
        hosts = list(subnet.hosts())
        if len(hosts) < 2:
            raise RuntimeError("link subnet too small")
        ip_a = f"{hosts[0]}/{subnet.prefixlen}"
        ip_b = f"{hosts[1]}/{subnet.prefixlen}"
        ip("addr", "add", ip_a, "dev", if_a, ns=ns_a)
        ip("addr", "add", ip_b, "dev", if_b, ns=ns_b)
        ip("link", "set", if_a, "up", ns=ns_a)
        ip("link", "set", if_b, "up", ns=ns_b)
        tune_netns_iface(ns_a, if_a)
        tune_netns_iface(ns_b, if_b)

        apply_tc(ns_a, if_a, e.get("tc", {}) or {})
        apply_tc(ns_b, if_b, e.get("tc", {}) or {})

        iface_for_link[(a, b)] = (if_a, if_b)
        iface_for_link[(b, a)] = (if_b, if_a)

    # generate config and start daemons
    for n in spec["nodes"]:
        name = n["name"]
        ns = ns_names[name]
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

            for i in range(next_if_index[name]):
                f.write(f"iface=eth{i}\n")

            for e in spec["links"]:
                if name != e["a"] and name != e["b"]:
                    continue
                peer = e["b"] if name == e["a"] else e["a"]
                if_local, _ = iface_for_link[(name, peer)]
                f.write(f"iface_cost={if_local}:{e['cost']}\n")

            for pfx in n.get("advertise", []):
                f.write(f"prefix={pfx}\n")

        dry = "--dry-run" if args.dry_run else ""
        cmd = f"nohup {romamd} --config {conf_path} {dry} >{log_path} 2>&1 & echo $! > {pid_path}"
        sh(["ip", "netns", "exec", ns, "sh", "-lc", cmd])

    print("*** waiting for convergence...")
    time.sleep(3)

    if args.show_routes:
        for n in spec["nodes"]:
            name = n["name"]
            ns = ns_names[name]
            res = sh(["ip", "-n", ns, "-4", "route", "show", "table", str(route_table)], capture=True)
            print(f"*** routes {name} table {route_table}:\n{(res.stdout or '').strip()}\n")

    if spec["nodes"] and (not args.no_ping):
        src = spec["nodes"][0]
        src_name = src["name"]
        src_ns = ns_names[src_name]
        for n in spec["nodes"][1:]:
            rid = n["router_id"]
            res = sh(["ip", "netns", "exec", src_ns, "ping", "-c", "1", "-W", "1", rid], check=False, capture=True)
            tail = "\n".join((res.stdout or "").splitlines()[-2:])
            print(f"*** ping {src_name} -> {n['name']} ({rid}):\n{tail}\n")

    try:
        if args.duration > 0:
            print(f"*** running for {args.duration}s...")
            time.sleep(args.duration)
            return
        print("*** done. press Ctrl-C to stop.")
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        print("*** stopping daemons...")
        for n in spec["nodes"]:
            name = n["name"]
            ns = ns_names[name]
            pid_path = os.path.join(tmpdir, f"{name}.pid")
            if os.path.exists(pid_path):
                try:
                    with open(pid_path, "r", encoding="utf-8") as f:
                        pid = f.read().strip()
                    if pid:
                        sh(["ip", "netns", "exec", ns, "kill", pid], check=False)
                except Exception:
                    pass
        print("*** deleting namespaces...")
        for ns in ns_names.values():
            sh(["ip", "netns", "del", ns], check=False)


if __name__ == "__main__":
    main()
