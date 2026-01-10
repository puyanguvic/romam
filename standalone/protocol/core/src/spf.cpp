#include "romam/spf.hpp"

#include <limits>
#include <queue>
#include <unordered_map>

namespace romam {
namespace {

struct NodeKeyHash {
  size_t operator()(const Ipv4Address& a) const noexcept { return static_cast<size_t>(a.ToU32NetworkOrder()); }
};

struct Edge {
  Ipv4Address to{};
  uint32_t cost{1};
};

using Graph = std::unordered_map<Ipv4Address, std::vector<Edge>, NodeKeyHash>;

static Graph BuildGraph(const std::vector<RomamLsa>& lsas) {
  Graph g;
  for (const auto& lsa : lsas) {
    auto& out = g[lsa.router_id];
    out.clear();
    out.reserve(lsa.links.size());
    for (const auto& link : lsa.links) {
      out.push_back(Edge{link.neighbor_id, link.cost});
    }
  }
  return g;
}

}  // namespace

std::map<Ipv4Address, NextHop> ComputeSpf(const Ipv4Address& self, const std::vector<RomamLsa>& lsas) {
  const auto graph = BuildGraph(lsas);

  struct QItem {
    Ipv4Address node{};
    uint32_t dist{0};
    bool operator>(const QItem& other) const { return dist > other.dist; }
  };

  std::unordered_map<Ipv4Address, uint32_t, NodeKeyHash> dist;
  std::unordered_map<Ipv4Address, Ipv4Address, NodeKeyHash> prev;

  for (const auto& [node, _] : graph) dist[node] = std::numeric_limits<uint32_t>::max();
  dist[self] = 0;

  std::priority_queue<QItem, std::vector<QItem>, std::greater<QItem>> pq;
  pq.push(QItem{self, 0});

  while (!pq.empty()) {
    const auto cur = pq.top();
    pq.pop();
    auto it_dist = dist.find(cur.node);
    if (it_dist == dist.end() || cur.dist != it_dist->second) continue;

    auto it_adj = graph.find(cur.node);
    if (it_adj == graph.end()) continue;

    for (const auto& e : it_adj->second) {
      const uint32_t nd = cur.dist + e.cost;
      auto it_best = dist.find(e.to);
      if (it_best == dist.end() || nd < it_best->second) {
        dist[e.to] = nd;
        prev[e.to] = cur.node;
        pq.push(QItem{e.to, nd});
      }
    }
  }

  std::map<Ipv4Address, NextHop> out;
  for (const auto& [node, d] : dist) {
    if (node == self) continue;
    if (d == std::numeric_limits<uint32_t>::max()) continue;

    Ipv4Address step = node;
    Ipv4Address parent = {};
    while (true) {
      auto it = prev.find(step);
      if (it == prev.end()) break;
      parent = it->second;
      if (parent == self) break;
      step = parent;
    }
    if (parent != self) continue;

    out.emplace(node, NextHop{step, d});
  }
  return out;
}

}  // namespace romam
