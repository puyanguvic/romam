#pragma once

#include "romam/ip.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace romam {

struct LinuxRoute {
  Ipv4Prefix dst{};
  std::optional<Ipv4Address> gateway;
  std::optional<Ipv4Address> preferred_source;
  int ifindex{0};
  uint32_t metric{100};
  uint8_t table{254};
};

class LinuxNetlink {
 public:
  LinuxNetlink();
  ~LinuxNetlink();

  LinuxNetlink(const LinuxNetlink&) = delete;
  LinuxNetlink& operator=(const LinuxNetlink&) = delete;

  bool ReplaceRoute(const LinuxRoute& route, std::string* error);
  bool DeleteRoute(const LinuxRoute& route, std::string* error);

 private:
  int m_fd{-1};
  uint32_t m_seq{0};
};

}  // namespace romam
