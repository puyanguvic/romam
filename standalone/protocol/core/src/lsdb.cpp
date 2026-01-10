#include "romam/lsdb.hpp"

namespace romam {

bool Lsdb::InstallLsa(const RomamLsa& lsa) {
  auto it = m_lsas.find(lsa.router_id);
  if (it == m_lsas.end()) {
    m_lsas.emplace(lsa.router_id, lsa);
    return true;
  }
  if (lsa.seq <= it->second.seq) return false;
  it->second = lsa;
  return true;
}

std::optional<RomamLsa> Lsdb::GetLsa(const Ipv4Address& router_id) const {
  auto it = m_lsas.find(router_id);
  if (it == m_lsas.end()) return std::nullopt;
  return it->second;
}

std::vector<RomamLsa> Lsdb::AllLsas() const {
  std::vector<RomamLsa> out;
  out.reserve(m_lsas.size());
  for (const auto& [_, lsa] : m_lsas) out.push_back(lsa);
  return out;
}

}  // namespace romam
