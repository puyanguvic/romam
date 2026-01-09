#include "multicast.hpp"

#include <arpa/inet.h>
#include <net/if.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace romam {
namespace {

static std::string ErrnoStr() { return std::string(std::strerror(errno)); }

}  // namespace

MulticastSocket::MulticastSocket(Ipv4Address group, uint16_t port) : m_group(group), m_port(port) {
  m_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (m_fd < 0) return;

  int one = 1;
  (void)setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  (void)setsockopt(m_fd, IPPROTO_IP, IP_PKTINFO, &one, sizeof(one));
  uint8_t ttl = 1;
  (void)setsockopt(m_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
  uint8_t loop = 0;
  (void)setsockopt(m_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

  sockaddr_in bind_addr{};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_port = htons(m_port);
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(m_fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(bind_addr)) < 0) {
    close(m_fd);
    m_fd = -1;
    return;
  }
}

MulticastSocket::~MulticastSocket() {
  if (m_fd >= 0) close(m_fd);
}

bool MulticastSocket::JoinOnInterface(int ifindex, std::string* error) {
  if (m_fd < 0) {
    if (error) *error = "socket init failed";
    return false;
  }
  ip_mreqn mreq{};
  mreq.imr_multiaddr.s_addr = m_group.ToU32NetworkOrder();
  mreq.imr_address.s_addr = htonl(INADDR_ANY);
  mreq.imr_ifindex = ifindex;
  if (setsockopt(m_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
    if (error) *error = "IP_ADD_MEMBERSHIP: " + ErrnoStr();
    return false;
  }
  return true;
}

bool MulticastSocket::SendOnInterface(int ifindex, const uint8_t* data, size_t len, std::string* error) {
  if (m_fd < 0) {
    if (error) *error = "socket init failed";
    return false;
  }

  ip_mreqn mreq{};
  mreq.imr_ifindex = ifindex;
  if (setsockopt(m_fd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0) {
    if (error) *error = "IP_MULTICAST_IF: " + ErrnoStr();
    return false;
  }

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(m_port);
  dst.sin_addr.s_addr = m_group.ToU32NetworkOrder();

  if (sendto(m_fd, data, len, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) < 0) {
    if (error) *error = "sendto: " + ErrnoStr();
    return false;
  }
  return true;
}

std::optional<RxPacketInfo> MulticastSocket::Recv(std::string* error) {
  if (m_fd < 0) {
    if (error) *error = "socket init failed";
    return std::nullopt;
  }

  pollfd pfd{};
  pfd.fd = m_fd;
  pfd.events = POLLIN;
  const int pr = poll(&pfd, 1, 100);
  if (pr < 0) {
    if (error) *error = "poll: " + ErrnoStr();
    return std::nullopt;
  }
  if (pr == 0) {
    if (error) error->clear();
    return std::nullopt;
  }

  uint8_t buf[65535];
  alignas(cmsghdr) uint8_t cbuf[256];

  sockaddr_in src{};
  iovec iov{buf, sizeof(buf)};
  msghdr msg{};
  msg.msg_name = &src;
  msg.msg_namelen = sizeof(src);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cbuf;
  msg.msg_controllen = sizeof(cbuf);

  const ssize_t n = recvmsg(m_fd, &msg, 0);
  if (n < 0) {
    if (error) *error = "recvmsg: " + ErrnoStr();
    return std::nullopt;
  }

  int ifindex = -1;
  for (auto* c = CMSG_FIRSTHDR(&msg); c; c = CMSG_NXTHDR(&msg, c)) {
    if (c->cmsg_level == IPPROTO_IP && c->cmsg_type == IP_PKTINFO) {
      auto* info = reinterpret_cast<in_pktinfo*>(CMSG_DATA(c));
      ifindex = info->ipi_ifindex;
    }
  }

  RxPacketInfo out{};
  out.ifindex = ifindex;
  out.src_ip = Ipv4Address::FromU32NetworkOrder(static_cast<uint32_t>(src.sin_addr.s_addr));
  out.payload.assign(buf, buf + n);
  return out;
}

}  // namespace romam
