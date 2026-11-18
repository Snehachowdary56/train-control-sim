// Small TCP socket wrapper that builds on Windows (Winsock2) and Linux/macOS (POSIX sockets).
#pragma once
#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
#else
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
#endif

namespace net {

// Winsock must be started once per process before any socket call (no-op on POSIX).
class NetworkInit {
 public:
  NetworkInit();
  ~NetworkInit();
  NetworkInit(const NetworkInit&) = delete;
  NetworkInit& operator=(const NetworkInit&) = delete;
};

class Socket {
 public:
  Socket() = default;
  explicit Socket(socket_t s) : s_(s) {}
  ~Socket() { close(); }
  Socket(Socket&& o) noexcept : s_(o.s_), buf_(std::move(o.buf_)) { o.s_ = kInvalidSocket; }
  Socket& operator=(Socket&& o) noexcept;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  static Socket listen_loopback(uint16_t port);  // port 0 = let the OS pick a free port
  static Socket connect_loopback(uint16_t port);
  uint16_t local_port() const;
  Socket accept() const;
  bool send_all(const std::string& data) const;
  bool read_line(std::string& line);             // false on disconnect
  void set_recv_timeout_ms(int ms) const;
  void shutdown_both() const;                    // wakes up a thread blocked in accept()/recv()
  void close();
  bool valid() const { return s_ != kInvalidSocket; }

 private:
  socket_t s_ = kInvalidSocket;
  std::string buf_;
};

std::string last_error();

}  // namespace net
