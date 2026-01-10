#include "romam/config.hpp"

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace romam {
namespace {

static std::string Trim(std::string s) {
  auto is_space = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!s.empty() && is_space(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  while (!s.empty() && is_space(static_cast<unsigned char>(s.back()))) s.pop_back();
  return s;
}

static std::pair<std::string, std::string> SplitKeyValue(const std::string& line) {
  auto pos = line.find('=');
  if (pos == std::string::npos) return {Trim(line), ""};
  return {Trim(line.substr(0, pos)), Trim(line.substr(pos + 1))};
}

static std::optional<std::pair<std::string, std::string>> SplitOnce(const std::string& s, char c) {
  auto pos = s.find(c);
  if (pos == std::string::npos) return std::nullopt;
  return std::make_pair(s.substr(0, pos), s.substr(pos + 1));
}

static std::chrono::milliseconds ParseMs(const std::string& v, const char* key) {
  try {
    return std::chrono::milliseconds(std::stoll(v));
  } catch (...) {
    throw std::runtime_error(std::string("invalid ") + key + ": " + v);
  }
}

}  // namespace

RomamConfig LoadConfigFile(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("failed to open config: " + path);

  RomamConfig cfg;
  std::string line;
  int line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    line = Trim(line);
    if (line.empty() || line[0] == '#') continue;

    auto [key, value] = SplitKeyValue(line);
    if (key.empty()) continue;

    if (key == "router_id") {
      auto parsed = Ipv4Address::Parse(value);
      if (!parsed) throw std::runtime_error("invalid router_id at line " + std::to_string(line_no));
      cfg.router_id = *parsed;
      continue;
    }

    if (key == "loopback") {
      auto parsed = Ipv4Prefix::Parse(value);
      if (!parsed) throw std::runtime_error("invalid loopback at line " + std::to_string(line_no));
      cfg.loopback = *parsed;
      continue;
    }

    if (key == "iface") {
      if (value.empty()) throw std::runtime_error("empty iface at line " + std::to_string(line_no));
      cfg.interfaces.push_back(InterfaceConfig{value});
      continue;
    }

    if (key == "iface_cost") {
      auto parts = SplitOnce(value, ':');
      if (!parts) throw std::runtime_error("invalid iface_cost at line " + std::to_string(line_no));
      const auto ifname = Trim(parts->first);
      const auto cost_s = Trim(parts->second);
      if (ifname.empty() || cost_s.empty()) {
        throw std::runtime_error("invalid iface_cost at line " + std::to_string(line_no));
      }
      cfg.iface_cost[ifname] = static_cast<uint32_t>(std::stoul(cost_s));
      continue;
    }

    if (key == "prefix") {
      auto parsed = Ipv4Prefix::Parse(value);
      if (!parsed) throw std::runtime_error("invalid prefix at line " + std::to_string(line_no));
      cfg.advertise_prefixes.push_back(PrefixConfig{*parsed});
      continue;
    }

    if (key == "multicast") {
      auto pos = value.find(':');
      if (pos == std::string::npos) throw std::runtime_error("invalid multicast at line " + std::to_string(line_no));
      auto ip = Ipv4Address::Parse(value.substr(0, pos));
      if (!ip) throw std::runtime_error("invalid multicast ip at line " + std::to_string(line_no));
      cfg.multicast.group = *ip;
      cfg.multicast.port = static_cast<uint16_t>(std::stoul(value.substr(pos + 1)));
      continue;
    }

    if (key == "hello_interval_ms") {
      cfg.hello_interval = ParseMs(value, "hello_interval_ms");
      continue;
    }

    if (key == "dead_interval_ms") {
      cfg.dead_interval = ParseMs(value, "dead_interval_ms");
      continue;
    }

    if (key == "lsa_interval_ms") {
      cfg.lsa_interval = ParseMs(value, "lsa_interval_ms");
      continue;
    }

    if (key == "route_table") {
      const unsigned long t = std::stoul(value);
      if (t > 255) throw std::runtime_error("route_table must be 0-255");
      cfg.route_table = static_cast<uint8_t>(t);
      continue;
    }

    if (key == "route_metric_base") {
      cfg.route_metric_base = static_cast<uint32_t>(std::stoul(value));
      continue;
    }

    if (key == "routing_algo") {
      if (value.empty()) throw std::runtime_error("empty routing_algo at line " + std::to_string(line_no));
      cfg.routing_algo = value;
      continue;
    }

    throw std::runtime_error("unknown key at line " + std::to_string(line_no) + ": " + key);
  }

  if (cfg.router_id == Ipv4Address(0, 0, 0, 0)) {
    throw std::runtime_error("router_id is required");
  }
  if (cfg.interfaces.empty()) {
    throw std::runtime_error("at least one iface is required");
  }
  return cfg;
}

}  // namespace romam
