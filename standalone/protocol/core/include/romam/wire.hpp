#pragma once

#include "romam/ip.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace romam {

enum class MsgType : uint16_t {
  Hello = 1,
  Lsa = 2,
};

struct RomamHello {
  Ipv4Address router_id{};
  Ipv4Address src_ip{};
};

struct RomamLink {
  Ipv4Address neighbor_id{};
  uint32_t cost{1};
};

struct RomamPrefix {
  Ipv4Prefix prefix{};
};

struct RomamLsa {
  Ipv4Address router_id{};
  uint32_t seq{0};
  std::vector<RomamLink> links;
  std::vector<RomamPrefix> prefixes;
};

struct DecodedMessage {
  MsgType type{};
  RomamHello hello{};
  RomamLsa lsa{};
};

std::vector<uint8_t> EncodeHello(const RomamHello& hello);
std::vector<uint8_t> EncodeLsa(const RomamLsa& lsa);
std::optional<DecodedMessage> DecodeMessage(const uint8_t* data, size_t len, std::string* error);

}  // namespace romam
