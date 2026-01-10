#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace romam {

class Ipv4Address {
 public:
  constexpr Ipv4Address() = default;
  constexpr Ipv4Address(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : m_bytes{a, b, c, d} {}

  static std::optional<Ipv4Address> Parse(const std::string& text);
  std::string ToString() const;

  uint32_t ToU32NetworkOrder() const;
  uint32_t ToU32HostOrder() const;
  static Ipv4Address FromU32NetworkOrder(uint32_t be32);

  const std::array<uint8_t, 4>& Bytes() const { return m_bytes; }

  friend bool operator==(const Ipv4Address& a, const Ipv4Address& b) { return a.m_bytes == b.m_bytes; }
  friend bool operator!=(const Ipv4Address& a, const Ipv4Address& b) { return !(a == b); }
  friend bool operator<(const Ipv4Address& a, const Ipv4Address& b) { return a.m_bytes < b.m_bytes; }

 private:
  std::array<uint8_t, 4> m_bytes{0, 0, 0, 0};
};

struct Ipv4Prefix {
  Ipv4Address network{};
  uint8_t prefix_len{0};

  static std::optional<Ipv4Prefix> Parse(const std::string& text);
  std::string ToString() const;
};

}  // namespace romam
