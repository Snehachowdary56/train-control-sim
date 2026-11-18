#include "net.hpp"

#include <cstring>
#include <stdexcept>

#ifndef _WIN32
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace net {

#ifdef _WIN32
NetworkInit::NetworkInit() {
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed");
}
NetworkInit::~NetworkInit() { WSACleanup(); }
std::string last_error() { return "Winsock error " + std::to_string(WSAGetLastError()); }
static void close_socket(socket_t s) { closesocket(s); }
#else
NetworkInit::NetworkInit() = default;
NetworkInit::~NetworkInit() = default;
std::string last_error() { return std::strerror(errno); }
static void close_socket(socket_t s) { ::close(s); }
#endif

Socket& Socket::operator=(Socket&& o) noexcept {
  if (this != &o) {
    close();
    s_ = o.s_;
    buf_ = std::move(o.buf_);
    o.s_ = kInvalidSocket;
  }
  return *this;
}

void Socket::close() {
  if (s_ != kInvalidSocket) {
    close_socket(s_);
    s_ = kInvalidSocket;
  }
}

static sockaddr_in loopback(uint16_t port) {
  sockaddr_in a{};
  a.sin_family = AF_INET;
  a.sin_port = htons(port);
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return a;
}

static void no_delay(socket_t s) {
  int one = 1;  // send small messages immediately (low latency for control messages)
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof one);
}

Socket Socket::listen_loopback(uint16_t port) {
  Socket s(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  if (!s.valid()) throw std::runtime_error("socket: " + last_error());
  int one = 1;
  setsockopt(s.s_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof one);
  auto addr = loopback(port);
  if (::bind(s.s_, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) throw std::runtime_error("bind: " + last_error());
  if (::listen(s.s_, 16) != 0) throw std::runtime_error("listen: " + last_error());
  return s;
}

Socket Socket::connect_loopback(uint16_t port) {
  Socket s(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  if (!s.valid()) throw std::runtime_error("socket: " + last_error());
  auto addr = loopback(port);
  if (::connect(s.s_, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0) throw std::runtime_error("connect: " + last_error());
  no_delay(s.s_);
  return s;
}

uint16_t Socket::local_port() const {
  sockaddr_in a{};
#ifdef _WIN32
  int len = sizeof a;
#else
  socklen_t len = sizeof a;
#endif
  getsockname(s_, reinterpret_cast<sockaddr*>(&a), &len);
  return ntohs(a.sin_port);
}

Socket Socket::accept() const {
  Socket c(::accept(s_, nullptr, nullptr));
  if (c.valid()) no_delay(c.s_);
  return c;
}

bool Socket::send_all(const std::string& data) const {
  size_t sent = 0;
  while (sent < data.size()) {
    auto n = ::send(s_, data.data() + sent, static_cast<int>(data.size() - sent), 0);
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

bool Socket::read_line(std::string& line) {
  for (;;) {
    auto nl = buf_.find('\n');
    if (nl != std::string::npos) {
      line = buf_.substr(0, nl);
      buf_.erase(0, nl + 1);
      return true;
    }
    char chunk[512];
    auto n = ::recv(s_, chunk, sizeof chunk, 0);
    if (n <= 0) return false;  // closed, error or timeout
    buf_.append(chunk, static_cast<size_t>(n));
  }
}

void Socket::set_recv_timeout_ms(int ms) const {
#ifdef _WIN32
  DWORD tv = static_cast<DWORD>(ms);
#else
  timeval tv{ms / 1000, (ms % 1000) * 1000};
#endif
  setsockopt(s_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof tv);
}

void Socket::shutdown_both() const {
#ifdef _WIN32
  ::shutdown(s_, SD_BOTH);
#else
  ::shutdown(s_, SHUT_RDWR);
#endif
}

}  // namespace net
