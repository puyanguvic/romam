#pragma once

#include "romam/ip.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace romam {

struct InterfaceConfig {
  std::string name;
};

struct PrefixConfig {
  Ipv4Prefix prefix;
};

struct MulticastConfig {
  Ipv4Address group{239, 255, 0, 1};
  uint16_t port{5000};
};

struct RomamConfig {
  Ipv4Address router_id{0, 0, 0, 0};
  std::optional<Ipv4Prefix> loopback;
  std::vector<InterfaceConfig> interfaces;
  std::unordered_map<std::string, uint32_t> iface_cost;
  std::vector<PrefixConfig> advertise_prefixes;

  MulticastConfig multicast{};
  std::chrono::milliseconds hello_interval{1000};
  std::chrono::milliseconds dead_interval{5000};
  std::chrono::milliseconds lsa_interval{3000};

  uint8_t route_table{100};
  uint32_t route_metric_base{100};
};

RomamConfig LoadConfigFile(const std::string& path);

}  // namespace romam
