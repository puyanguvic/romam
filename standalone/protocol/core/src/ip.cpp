#include "romam/ip.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <sstream>

namespace romam {

std::optional<Ipv4Address> Ipv4Address::Parse(const std::string& text) {
  in_addr addr{};
  if (inet_pton(AF_INET, text.c_str(), &addr) != 1) return std::nullopt;
  return Ipv4Address::FromU32NetworkOrder(static_cast<uint32_t>(addr.s_addr));
}

std::string Ipv4Address::ToString() const {
  char buf[INET_ADDRSTRLEN] = {0};
  in_addr addr{};
  addr.s_addr = static_cast<in_addr_t>(ToU32NetworkOrder());
  if (!inet_ntop(AF_INET, &addr, buf, sizeof(buf))) return "0.0.0.0";
  return std::string(buf);
}

uint32_t Ipv4Address::ToU32NetworkOrder() const {
  uint32_t be32 = 0;
  std::memcpy(&be32, m_bytes.data(), 4);
  return be32;
}

Ipv4Address Ipv4Address::FromU32NetworkOrder(uint32_t be32) {
  Ipv4Address out;
  std::memcpy(out.m_bytes.data(), &be32, 4);
  return out;
}

uint32_t Ipv4Address::ToU32HostOrder() const { return ntohl(ToU32NetworkOrder()); }

std::optional<Ipv4Prefix> Ipv4Prefix::Parse(const std::string& text) {
  auto pos = text.find('/');
  if (pos == std::string::npos) return std::nullopt;
  auto ip = Ipv4Address::Parse(text.substr(0, pos));
  if (!ip) return std::nullopt;
  int len = -1;
  try {
    len = std::stoi(text.substr(pos + 1));
  } catch (...) {
    return std::nullopt;
  }
  if (len < 0 || len > 32) return std::nullopt;
  return Ipv4Prefix{*ip, static_cast<uint8_t>(len)};
}

std::string Ipv4Prefix::ToString() const { return network.ToString() + "/" + std::to_string(prefix_len); }

}  // namespace romam
