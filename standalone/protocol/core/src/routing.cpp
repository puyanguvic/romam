#include "romam/routing.hpp"

#include "romam/spf.hpp"

#include <stdexcept>

namespace romam {

std::map<Ipv4Address, NextHop> ComputeRoutes(const std::string& algo,
                                            const Ipv4Address& self,
                                            const std::vector<RomamLsa>& lsas) {
  if (algo == "spf") return ComputeSpf(self, lsas);
  throw std::runtime_error("unknown routing_algo: " + algo);
}

}  // namespace romam

