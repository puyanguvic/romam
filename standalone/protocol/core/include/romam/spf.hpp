#pragma once

#include "romam/ip.hpp"
#include "romam/wire.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

namespace romam {

struct NextHop {
  Ipv4Address next_hop_router{};
  uint32_t cost{0};
};

std::map<Ipv4Address, NextHop> ComputeSpf(const Ipv4Address& self, const std::vector<RomamLsa>& lsas);

}  // namespace romam
