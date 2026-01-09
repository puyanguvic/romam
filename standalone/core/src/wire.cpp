#include "romam/wire.hpp"

#include <arpa/inet.h>

#include <cstring>
#include <string>

namespace romam {
namespace {

constexpr uint32_t kMagic = 0x524f4d41;  // "ROMA"
constexpr uint16_t kVersion = 1;

struct __attribute__((packed)) Header {
  uint32_t magic;
  uint16_t version;
  uint16_t type;
  uint32_t length;
};

static void PushU16(std::vector<uint8_t>& out, uint16_t v) {
  uint16_t be = htons(v);
  auto* p = reinterpret_cast<const uint8_t*>(&be);
  out.insert(out.end(), p, p + sizeof(be));
}

static void PushU32(std::vector<uint8_t>& out, uint32_t v) {
  uint32_t be = htonl(v);
  auto* p = reinterpret_cast<const uint8_t*>(&be);
  out.insert(out.end(), p, p + sizeof(be));
}

static void PushBe32(std::vector<uint8_t>& out, uint32_t be32) {
  auto* p = reinterpret_cast<const uint8_t*>(&be32);
  out.insert(out.end(), p, p + sizeof(be32));
}

static std::optional<uint16_t> ReadU16(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 2) return std::nullopt;
  uint16_t be{};
  std::memcpy(&be, p, 2);
  p += 2;
  return ntohs(be);
}

static std::optional<uint32_t> ReadU32(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 4) return std::nullopt;
  uint32_t be{};
  std::memcpy(&be, p, 4);
  p += 4;
  return ntohl(be);
}

static std::optional<uint32_t> ReadBe32(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 4) return std::nullopt;
  uint32_t be{};
  std::memcpy(&be, p, 4);
  p += 4;
  return be;
}

}  // namespace

std::vector<uint8_t> EncodeHello(const RomamHello& hello) {
  std::vector<uint8_t> payload;
  PushBe32(payload, hello.router_id.ToU32NetworkOrder());
  PushBe32(payload, hello.src_ip.ToU32NetworkOrder());

  Header h{};
  h.magic = htonl(kMagic);
  h.version = htons(kVersion);
  h.type = htons(static_cast<uint16_t>(MsgType::Hello));
  h.length = htonl(static_cast<uint32_t>(payload.size()));

  std::vector<uint8_t> out(sizeof(Header));
  std::memcpy(out.data(), &h, sizeof(h));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

std::vector<uint8_t> EncodeLsa(const RomamLsa& lsa) {
  std::vector<uint8_t> payload;
  PushBe32(payload, lsa.router_id.ToU32NetworkOrder());
  PushU32(payload, lsa.seq);

  PushU16(payload, static_cast<uint16_t>(lsa.links.size()));
  for (const auto& link : lsa.links) {
    PushBe32(payload, link.neighbor_id.ToU32NetworkOrder());
    PushU32(payload, link.cost);
  }

  PushU16(payload, static_cast<uint16_t>(lsa.prefixes.size()));
  for (const auto& pfx : lsa.prefixes) {
    PushBe32(payload, pfx.prefix.network.ToU32NetworkOrder());
    payload.push_back(pfx.prefix.prefix_len);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
  }

  Header h{};
  h.magic = htonl(kMagic);
  h.version = htons(kVersion);
  h.type = htons(static_cast<uint16_t>(MsgType::Lsa));
  h.length = htonl(static_cast<uint32_t>(payload.size()));

  std::vector<uint8_t> out(sizeof(Header));
  std::memcpy(out.data(), &h, sizeof(h));
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

std::optional<DecodedMessage> DecodeMessage(const uint8_t* data, size_t len, std::string* error) {
  if (len < sizeof(Header)) {
    if (error) *error = "short header";
    return std::nullopt;
  }

  Header h{};
  std::memcpy(&h, data, sizeof(h));
  if (ntohl(h.magic) != kMagic) {
    if (error) *error = "bad magic";
    return std::nullopt;
  }
  if (ntohs(h.version) != kVersion) {
    if (error) *error = "bad version";
    return std::nullopt;
  }

  const uint16_t type = ntohs(h.type);
  const uint32_t plen = ntohl(h.length);
  if (sizeof(Header) + plen != len) {
    if (error) *error = "bad length";
    return std::nullopt;
  }

  const uint8_t* p = data + sizeof(Header);
  const uint8_t* end = p + plen;

  DecodedMessage out{};
  out.type = static_cast<MsgType>(type);

  if (out.type == MsgType::Hello) {
    auto rid = ReadBe32(p, end);
    auto sip = ReadBe32(p, end);
    if (!rid || !sip || p != end) {
      if (error) *error = "bad hello payload";
      return std::nullopt;
    }
    out.hello.router_id = Ipv4Address::FromU32NetworkOrder(*rid);
    out.hello.src_ip = Ipv4Address::FromU32NetworkOrder(*sip);
    return out;
  }

  if (out.type == MsgType::Lsa) {
    auto rid = ReadBe32(p, end);
    auto seq = ReadU32(p, end);
    if (!rid || !seq) {
      if (error) *error = "bad lsa header";
      return std::nullopt;
    }
    out.lsa.router_id = Ipv4Address::FromU32NetworkOrder(*rid);
    out.lsa.seq = *seq;

    auto nlinks = ReadU16(p, end);
    if (!nlinks) {
      if (error) *error = "bad lsa links count";
      return std::nullopt;
    }
    out.lsa.links.reserve(*nlinks);
    for (uint16_t i = 0; i < *nlinks; ++i) {
      auto nid = ReadBe32(p, end);
      auto cost = ReadU32(p, end);
      if (!nid || !cost) {
        if (error) *error = "bad lsa link";
        return std::nullopt;
      }
      out.lsa.links.push_back(RomamLink{Ipv4Address::FromU32NetworkOrder(*nid), *cost});
    }

    auto npfx = ReadU16(p, end);
    if (!npfx) {
      if (error) *error = "bad lsa prefixes count";
      return std::nullopt;
    }
    out.lsa.prefixes.reserve(*npfx);
    for (uint16_t i = 0; i < *npfx; ++i) {
      auto net = ReadBe32(p, end);
      if (!net || end - p < 4) {
        if (error) *error = "bad lsa prefix";
        return std::nullopt;
      }
      uint8_t plen8 = p[0];
      p += 4;
      if (plen8 > 32) {
        if (error) *error = "bad lsa prefix len";
        return std::nullopt;
      }
      out.lsa.prefixes.push_back(RomamPrefix{Ipv4Prefix{Ipv4Address::FromU32NetworkOrder(*net), plen8}});
    }

    if (p != end) {
      if (error) *error = "trailing bytes";
      return std::nullopt;
    }
    return out;
  }

  if (error) *error = "unknown type";
  return std::nullopt;
}

}  // namespace romam
