#pragma once

#include "romam/ip.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace romam {

struct RxPacketInfo {
  int ifindex{-1};
  Ipv4Address src_ip{};
  std::vector<uint8_t> payload;
};

class MulticastSocket {
 public:
  MulticastSocket(Ipv4Address group, uint16_t port);
  ~MulticastSocket();

  MulticastSocket(const MulticastSocket&) = delete;
  MulticastSocket& operator=(const MulticastSocket&) = delete;

  bool JoinOnInterface(int ifindex, std::string* error);
  bool SendOnInterface(int ifindex, const uint8_t* data, size_t len, std::string* error);
  std::optional<RxPacketInfo> Recv(std::string* error);

 private:
  int m_fd{-1};
  Ipv4Address m_group{};
  uint16_t m_port{0};
};

}  // namespace romam
