#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Endpoint {
  std::string ip;
  uint16_t port{0};
};

static std::optional<Endpoint> ParseEndpoint(const std::string& s) {
  const auto pos = s.rfind(':');
  if (pos == std::string::npos) return std::nullopt;
  Endpoint e;
  e.ip = s.substr(0, pos);
  try {
    e.port = static_cast<uint16_t>(std::stoul(s.substr(pos + 1)));
  } catch (...) {
    return std::nullopt;
  }
  if (e.ip.empty() || e.port == 0) return std::nullopt;
  return e;
}

static sockaddr_in ToSockaddr(const Endpoint& e) {
  sockaddr_in sa{};
  sa.sin_family = AF_INET;
  sa.sin_port = htons(e.port);
  if (inet_pton(AF_INET, e.ip.c_str(), &sa.sin_addr) != 1) {
    throw std::runtime_error("invalid IPv4 address: " + e.ip);
  }
  return sa;
}

[[noreturn]] static void DieUsage(const std::string& msg) {
  if (!msg.empty()) std::cerr << "error: " << msg << "\n";
  std::cerr << "usage:\n"
            << "  romam-traffic --mode server --proto {udp|tcp} --bind <ip:port> --duration <sec>\n"
            << "  romam-traffic --mode client --proto {udp|tcp} --connect <ip:port> --duration <sec> [--size N]\n"
            << "              [--rate-mbps R] (udp only)\n";
  std::exit(2);
}

struct Args {
  std::string mode;   // server|client
  std::string proto;  // udp|tcp
  std::optional<Endpoint> bind;
  std::optional<Endpoint> connect;
  int duration_s{10};
  size_t payload_size{1200};
  double rate_mbps{10.0};  // udp only
};

static Args ParseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    std::string k = argv[i];
    auto need = [&](const char* opt) -> std::string {
      if (i + 1 >= argc) DieUsage(std::string("missing value for ") + opt);
      return argv[++i];
    };
    if (k == "--mode") {
      a.mode = need("--mode");
      continue;
    }
    if (k == "--proto") {
      a.proto = need("--proto");
      continue;
    }
    if (k == "--bind") {
      const auto v = need("--bind");
      a.bind = ParseEndpoint(v);
      if (!a.bind) DieUsage("invalid --bind endpoint");
      continue;
    }
    if (k == "--connect") {
      const auto v = need("--connect");
      a.connect = ParseEndpoint(v);
      if (!a.connect) DieUsage("invalid --connect endpoint");
      continue;
    }
    if (k == "--duration") {
      a.duration_s = std::stoi(need("--duration"));
      continue;
    }
    if (k == "--size") {
      a.payload_size = static_cast<size_t>(std::stoul(need("--size")));
      continue;
    }
    if (k == "--rate-mbps") {
      a.rate_mbps = std::stod(need("--rate-mbps"));
      continue;
    }
    DieUsage(std::string("unknown arg: ") + k);
  }
  if (a.mode != "server" && a.mode != "client") DieUsage("mode must be server|client");
  if (a.proto != "udp" && a.proto != "tcp") DieUsage("proto must be udp|tcp");
  if (a.duration_s <= 0) DieUsage("duration must be > 0");
  if (a.payload_size < 1) DieUsage("size must be >= 1");
  if (a.mode == "server" && !a.bind) DieUsage("server requires --bind");
  if (a.mode == "client" && !a.connect) DieUsage("client requires --connect");
  if (a.proto == "udp" && a.rate_mbps <= 0.0) DieUsage("rate-mbps must be > 0");
  return a;
}

static void PrintRate(const char* label, const std::chrono::steady_clock::time_point& start, uint64_t bytes_total) {
  const auto now = std::chrono::steady_clock::now();
  const double secs = std::chrono::duration<double>(now - start).count();
  const double mbps = secs > 0 ? (bytes_total * 8.0) / (secs * 1e6) : 0.0;
  std::cerr << label << " bytes=" << bytes_total << " mbps=" << mbps << "\n";
}

static int RunUdpServer(const Endpoint& bind, int duration_s) {
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) throw std::runtime_error("socket() failed");

  const int one = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  auto sa = ToSockaddr(bind);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
    throw std::runtime_error(std::string("bind() failed: ") + std::strerror(errno));
  }

  std::vector<uint8_t> buf(64 * 1024);
  uint64_t bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  const auto end = start + std::chrono::seconds(duration_s);
  while (std::chrono::steady_clock::now() < end) {
    const ssize_t n = ::recvfrom(fd, buf.data(), buf.size(), 0, nullptr, nullptr);
    if (n > 0) bytes += static_cast<uint64_t>(n);
  }
  ::close(fd);
  PrintRate("udp_server", start, bytes);
  return 0;
}

static int RunUdpClient(const Endpoint& dst, int duration_s, size_t payload_size, double rate_mbps) {
  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) throw std::runtime_error("socket() failed");

  auto sa = ToSockaddr(dst);

  std::vector<uint8_t> payload(payload_size, 0xab);
  uint64_t bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  const auto end = start + std::chrono::seconds(duration_s);

  const double bytes_per_sec = (rate_mbps * 1e6) / 8.0;
  const double interval_s = static_cast<double>(payload_size) / bytes_per_sec;
  auto next_send = start;

  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= end) break;
    if (now < next_send) {
      ::usleep(1000);
      continue;
    }
    const ssize_t n = ::sendto(fd, payload.data(), payload.size(), 0, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));
    if (n > 0) bytes += static_cast<uint64_t>(n);
    next_send += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(interval_s));
  }

  ::close(fd);
  PrintRate("udp_client", start, bytes);
  return 0;
}

static int RunTcpServer(const Endpoint& bind, int duration_s) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) throw std::runtime_error("socket() failed");

  const int one = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  auto sa = ToSockaddr(bind);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
    throw std::runtime_error(std::string("bind() failed: ") + std::strerror(errno));
  }
  if (::listen(fd, 16) != 0) throw std::runtime_error(std::string("listen() failed: ") + std::strerror(errno));

  const int cfd = ::accept(fd, nullptr, nullptr);
  if (cfd < 0) throw std::runtime_error(std::string("accept() failed: ") + std::strerror(errno));

  std::vector<uint8_t> buf(64 * 1024);
  uint64_t bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  const auto end = start + std::chrono::seconds(duration_s);
  while (std::chrono::steady_clock::now() < end) {
    const ssize_t n = ::recv(cfd, buf.data(), buf.size(), 0);
    if (n > 0) {
      bytes += static_cast<uint64_t>(n);
      continue;
    }
    if (n == 0) break;
  }

  ::close(cfd);
  ::close(fd);
  PrintRate("tcp_server", start, bytes);
  return 0;
}

static int RunTcpClient(const Endpoint& dst, int duration_s, size_t payload_size) {
  const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) throw std::runtime_error("socket() failed");

  auto sa = ToSockaddr(dst);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
    throw std::runtime_error(std::string("connect() failed: ") + std::strerror(errno));
  }

  std::vector<uint8_t> payload(payload_size, 0xcd);
  uint64_t bytes = 0;
  const auto start = std::chrono::steady_clock::now();
  const auto end = start + std::chrono::seconds(duration_s);
  while (std::chrono::steady_clock::now() < end) {
    const ssize_t n = ::send(fd, payload.data(), payload.size(), 0);
    if (n > 0) bytes += static_cast<uint64_t>(n);
  }
  ::shutdown(fd, SHUT_RDWR);
  ::close(fd);
  PrintRate("tcp_client", start, bytes);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const auto a = ParseArgs(argc, argv);
  try {
    if (a.proto == "udp" && a.mode == "server") return RunUdpServer(*a.bind, a.duration_s);
    if (a.proto == "udp" && a.mode == "client") return RunUdpClient(*a.connect, a.duration_s, a.payload_size, a.rate_mbps);
    if (a.proto == "tcp" && a.mode == "server") return RunTcpServer(*a.bind, a.duration_s);
    if (a.proto == "tcp" && a.mode == "client") return RunTcpClient(*a.connect, a.duration_s, a.payload_size);
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
