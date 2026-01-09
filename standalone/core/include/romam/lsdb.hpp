#pragma once

#include "romam/ip.hpp"
#include "romam/wire.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

namespace romam {

struct NeighborState {
  Ipv4Address neighbor_id{};
  Ipv4Address neighbor_ip{};
  int ifindex{-1};
  std::chrono::steady_clock::time_point last_seen{};
};

class Lsdb {
 public:
  bool InstallLsa(const RomamLsa& lsa);
  std::optional<RomamLsa> GetLsa(const Ipv4Address& router_id) const;
  std::vector<RomamLsa> AllLsas() const;

 private:
  std::map<Ipv4Address, RomamLsa> m_lsas;
};

}  // namespace romam

