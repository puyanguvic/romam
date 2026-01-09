#include "linux_netlink.hpp"

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

namespace romam {
namespace {

static void AppendAttr(nlmsghdr* nlh, size_t maxlen, int type, const void* data, size_t alen) {
  const size_t len = RTA_LENGTH(alen);
  const size_t newlen = NLMSG_ALIGN(nlh->nlmsg_len) + RTA_ALIGN(len);
  if (newlen > maxlen) return;
  auto* rta = reinterpret_cast<rtattr*>(reinterpret_cast<uint8_t*>(nlh) + NLMSG_ALIGN(nlh->nlmsg_len));
  rta->rta_type = type;
  rta->rta_len = len;
  std::memcpy(RTA_DATA(rta), data, alen);
  nlh->nlmsg_len = static_cast<uint32_t>(newlen);
}

static std::string ErrnoStr() { return std::string(std::strerror(errno)); }

static bool SendAndAck(int fd, nlmsghdr* req, std::string* error) {
  sockaddr_nl sa{};
  sa.nl_family = AF_NETLINK;

  iovec iov{req, req->nlmsg_len};
  msghdr msg{};
  msg.msg_name = &sa;
  msg.msg_namelen = sizeof(sa);
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  if (sendmsg(fd, &msg, 0) < 0) {
    if (error) *error = "sendmsg: " + ErrnoStr();
    return false;
  }

  alignas(nlmsghdr) uint8_t buf[8192];
  iovec riov{buf, sizeof(buf)};
  msghdr rmsg{};
  rmsg.msg_name = &sa;
  rmsg.msg_namelen = sizeof(sa);
  rmsg.msg_iov = &riov;
  rmsg.msg_iovlen = 1;

  ssize_t nread = recvmsg(fd, &rmsg, 0);
  if (nread < 0) {
    if (error) *error = "recvmsg: " + ErrnoStr();
    return false;
  }

  unsigned int remaining = static_cast<unsigned int>(nread);
  for (auto* nlh = reinterpret_cast<nlmsghdr*>(buf); NLMSG_OK(nlh, remaining); nlh = NLMSG_NEXT(nlh, remaining)) {
    if (nlh->nlmsg_type == NLMSG_ERROR) {
      auto* err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(nlh));
      if (err->error == 0) return true;
      errno = -err->error;
      if (error) *error = "netlink ack: " + ErrnoStr();
      return false;
    }
  }

  if (error) *error = "netlink: no ack";
  return false;
}

static bool RouteOp(int fd, uint32_t seq, int op, const LinuxRoute& route, std::string* error) {
  alignas(nlmsghdr) uint8_t buf[4096];
  std::memset(buf, 0, sizeof(buf));

  auto* nlh = reinterpret_cast<nlmsghdr*>(buf);
  nlh->nlmsg_len = NLMSG_LENGTH(sizeof(rtmsg));
  nlh->nlmsg_type = op;
  nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
  if (op == RTM_NEWROUTE) nlh->nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
  nlh->nlmsg_seq = seq;

  auto* rtm = reinterpret_cast<rtmsg*>(NLMSG_DATA(nlh));
  rtm->rtm_family = AF_INET;
  rtm->rtm_table = route.table;
  rtm->rtm_protocol = RTPROT_STATIC;
  rtm->rtm_scope = RT_SCOPE_UNIVERSE;
  rtm->rtm_type = RTN_UNICAST;
  rtm->rtm_dst_len = route.dst.prefix_len;

  const uint32_t dst_be = route.dst.network.ToU32NetworkOrder();
  AppendAttr(nlh, sizeof(buf), RTA_DST, &dst_be, sizeof(dst_be));

  if (route.gateway) {
    const uint32_t gw_be = route.gateway->ToU32NetworkOrder();
    AppendAttr(nlh, sizeof(buf), RTA_GATEWAY, &gw_be, sizeof(gw_be));
  }

  if (route.ifindex > 0) {
    AppendAttr(nlh, sizeof(buf), RTA_OIF, &route.ifindex, sizeof(route.ifindex));
  }

  if (route.metric != 0) {
    AppendAttr(nlh, sizeof(buf), RTA_PRIORITY, &route.metric, sizeof(route.metric));
  }

  return SendAndAck(fd, nlh, error);
}

}  // namespace

LinuxNetlink::LinuxNetlink() {
  m_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
}

LinuxNetlink::~LinuxNetlink() {
  if (m_fd >= 0) close(m_fd);
}

bool LinuxNetlink::ReplaceRoute(const LinuxRoute& route, std::string* error) {
  if (m_fd < 0) {
    if (error) *error = "netlink socket: " + ErrnoStr();
    return false;
  }
  return RouteOp(m_fd, ++m_seq, RTM_NEWROUTE, route, error);
}

bool LinuxNetlink::DeleteRoute(const LinuxRoute& route, std::string* error) {
  if (m_fd < 0) {
    if (error) *error = "netlink socket: " + ErrnoStr();
    return false;
  }
  return RouteOp(m_fd, ++m_seq, RTM_DELROUTE, route, error);
}

}  // namespace romam
