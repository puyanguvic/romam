#pragma once

#include "romam/spf.hpp"

#include <map>
#include <string>
#include <vector>

namespace romam {

std::map<Ipv4Address, NextHop> ComputeRoutes(const std::string& algo,
                                            const Ipv4Address& self,
                                            const std::vector<RomamLsa>& lsas);

}  // namespace romam
