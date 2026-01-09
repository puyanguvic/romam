#include "linux_netlink.hpp"
#include "multicast.hpp"

#include "romam/config.hpp"
#include "romam/lsdb.hpp"
#include "romam/spf.hpp"
#include "romam/wire.hpp"

#include <net/if.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace romam {
namespace {

static std::atomic<bool> g_stop{false};

static void OnSignal(int) { g_stop = true; }

static std::string RouteKey(const Ipv4Prefix& pfx) { return pfx.ToString(); }

struct InstalledRoute {
  LinuxRoute route{};
};

static void Log(const std::string& s) { std::cerr << s << "\n"; }

}  // namespace

int Main(int argc, char** argv) {
  std::string config_path;
  bool dry_run = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--config" && i + 1 < argc) {
      config_path = argv[++i];
      continue;
    }
    if (a == "--dry-run") {
      dry_run = true;
      continue;
    }
    std::cerr << "usage: romamd --config <path> [--dry-run]\n";
    return 2;
  }
  if (config_path.empty()) {
    std::cerr << "missing --config\n";
    return 2;
  }

  const auto cfg = LoadConfigFile(config_path);
  std::signal(SIGINT, OnSignal);
  std::signal(SIGTERM, OnSignal);

  std::vector<std::string> iface_names;
  for (const auto& iface : cfg.interfaces) {
    if (iface.name == "auto") {
      const size_t before = iface_names.size();
      struct if_nameindex* ifs = if_nameindex();
      if (!ifs) {
        iface_names.push_back("lo");
        continue;
      }
      for (auto* it = ifs; it && it->if_index != 0 && it->if_name != nullptr; ++it) {
        std::string name = it->if_name;
        if (name == "lo") continue;
        iface_names.push_back(name);
      }
      if_freenameindex(ifs);
      if (iface_names.size() == before) {
        iface_names.push_back("lo");
      }
      continue;
    }
    iface_names.push_back(iface.name);
  }

  if (iface_names.empty()) {
    std::cerr << "no usable iface (configure iface=... or iface=auto)\n";
    return 2;
  }

  std::vector<int> ifindices;
  for (const auto& name : iface_names) {
    const int ifi = static_cast<int>(if_nametoindex(name.c_str()));
    if (ifi <= 0) {
      std::cerr << "unknown iface: " << name << "\n";
      return 2;
    }
    ifindices.push_back(ifi);
  }

  std::unordered_map<int, uint32_t> if_cost;
  for (size_t i = 0; i < iface_names.size(); ++i) {
    uint32_t cost = 1;
    auto it = cfg.iface_cost.find(iface_names[i]);
    if (it != cfg.iface_cost.end()) cost = it->second;
    if_cost[ifindices[i]] = cost;
  }

  MulticastSocket mc(cfg.multicast.group, cfg.multicast.port);
  for (int ifi : ifindices) {
    std::string err;
    if (!mc.JoinOnInterface(ifi, &err)) {
      std::cerr << "multicast join failed (ifindex=" << ifi << "): " << err << "\n";
      return 1;
    }
  }

  Lsdb lsdb;
  std::map<Ipv4Address, NeighborState> neighbors;
  uint32_t self_seq = 1;

  auto make_self_lsa = [&]() -> RomamLsa {
    RomamLsa lsa{};
    lsa.router_id = cfg.router_id;
    lsa.seq = self_seq;
    for (const auto& [nid, st] : neighbors) {
      uint32_t cost = 1;
      auto it = if_cost.find(st.ifindex);
      if (it != if_cost.end()) cost = it->second;
      lsa.links.push_back(RomamLink{nid, cost});
    }
    if (cfg.loopback) lsa.prefixes.push_back(RomamPrefix{*cfg.loopback});
    for (const auto& p : cfg.advertise_prefixes) lsa.prefixes.push_back(RomamPrefix{p.prefix});
    return lsa;
  };

  auto flood = [&](const std::vector<uint8_t>& bytes) {
    for (int ifi : ifindices) {
      std::string err;
      (void)mc.SendOnInterface(ifi, bytes.data(), bytes.size(), &err);
    }
  };

  auto originate = [&]() {
    const auto lsa = make_self_lsa();
    lsdb.InstallLsa(lsa);
    flood(EncodeLsa(lsa));
  };

  std::unordered_map<std::string, InstalledRoute> installed;
  LinuxNetlink nl;

  auto recompute_and_program = [&]() {
    const auto lsas = lsdb.AllLsas();
    const auto nh = ComputeSpf(cfg.router_id, lsas);

    std::unordered_map<std::string, InstalledRoute> desired;
    for (const auto& lsa : lsas) {
      if (lsa.router_id == cfg.router_id) continue;
      auto it_nh = nh.find(lsa.router_id);
      if (it_nh == nh.end()) continue;
      const auto first_hop = it_nh->second.next_hop_router;
      auto it_nb = neighbors.find(first_hop);
      if (it_nb == neighbors.end()) continue;

      for (const auto& pfx : lsa.prefixes) {
        LinuxRoute r{};
        r.dst = pfx.prefix;
        r.gateway = it_nb->second.neighbor_ip;
        r.ifindex = it_nb->second.ifindex;
        r.metric = cfg.route_metric_base + it_nh->second.cost;
        r.table = cfg.route_table;
        desired.emplace(RouteKey(r.dst), InstalledRoute{r});
      }
    }

    for (const auto& [k, r] : desired) {
      const auto it = installed.find(k);
      if (it != installed.end()) continue;
      if (dry_run) {
        Log("ROUTE add " + r.route.dst.ToString() + " via " + r.route.gateway->ToString() + " dev " +
            std::to_string(r.route.ifindex) + " metric " + std::to_string(r.route.metric));
        continue;
      }
      std::string err;
      if (!nl.ReplaceRoute(r.route, &err)) Log("route replace failed for " + k + ": " + err);
    }

    for (const auto& [k, r] : installed) {
      if (desired.find(k) != desired.end()) continue;
      if (dry_run) {
        Log("ROUTE del " + r.route.dst.ToString());
        continue;
      }
      std::string err;
      (void)nl.DeleteRoute(r.route, &err);
    }

    installed = std::move(desired);
  };

  originate();

  auto last_hello = std::chrono::steady_clock::now();
  auto last_lsa = std::chrono::steady_clock::now();
  auto last_recompute = std::chrono::steady_clock::now();

  while (!g_stop.load()) {
    const auto now = std::chrono::steady_clock::now();

    if (now - last_hello >= cfg.hello_interval) {
      last_hello = now;
      RomamHello hello{};
      hello.router_id = cfg.router_id;
      hello.src_ip = Ipv4Address(0, 0, 0, 0);
      const auto bytes = EncodeHello(hello);
      flood(bytes);
    }

    if (now - last_lsa >= cfg.lsa_interval) {
      last_lsa = now;
      ++self_seq;
      originate();
    }

    bool neighbor_changed = false;
    for (auto it = neighbors.begin(); it != neighbors.end();) {
      if (now - it->second.last_seen > cfg.dead_interval) {
        Log("neighbor down: " + it->first.ToString());
        it = neighbors.erase(it);
        neighbor_changed = true;
        continue;
      }
      ++it;
    }
    if (neighbor_changed) {
      ++self_seq;
      originate();
    }

    std::string err;
    auto pkt = mc.Recv(&err);
    if (!pkt) {
      if (!err.empty()) Log("recv error: " + err);
      continue;
    }

    std::string perr;
    auto decoded = DecodeMessage(pkt->payload.data(), pkt->payload.size(), &perr);
    if (!decoded) continue;

    if (decoded->type == MsgType::Hello) {
      if (decoded->hello.router_id == cfg.router_id) continue;
      auto& st = neighbors[decoded->hello.router_id];
      const bool first = (st.last_seen.time_since_epoch().count() == 0);
      st.neighbor_id = decoded->hello.router_id;
      st.neighbor_ip = pkt->src_ip;
      st.ifindex = pkt->ifindex;
      st.last_seen = now;
      if (first) {
        Log("neighbor up: " + st.neighbor_id.ToString() + " via " + st.neighbor_ip.ToString() + " ifindex " +
            std::to_string(st.ifindex));
        ++self_seq;
        originate();
      }
      continue;
    }

    if (decoded->type == MsgType::Lsa) {
      if (decoded->lsa.router_id == cfg.router_id) continue;
      const bool changed = lsdb.InstallLsa(decoded->lsa);
      if (changed) {
        flood(EncodeLsa(decoded->lsa));
        recompute_and_program();
        last_recompute = now;
      } else if (now - last_recompute > std::chrono::milliseconds(500)) {
        recompute_and_program();
        last_recompute = now;
      }
      continue;
    }
  }

  return 0;
}

}  // namespace romam

int main(int argc, char** argv) {
  try {
    return romam::Main(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "fatal: " << e.what() << "\n";
    return 1;
  }
}
