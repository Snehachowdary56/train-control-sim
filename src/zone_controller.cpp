#include "zone_controller.hpp"

#include <sstream>

#include "protocol.hpp"

namespace cbtc {

ZoneController::ZoneController(TrainParams tp, LineParams lp) : tp_(tp), lp_(lp) {}
ZoneController::~ZoneController() { stop(); }

uint16_t ZoneController::start() {
  listener_ = net::Socket::listen_loopback(0);
  port_ = listener_.local_port();
  running_ = true;
  acceptor_ = std::thread([this] { accept_loop(); });
  return port_;
}

void ZoneController::accept_loop() {
  while (running_) {
    auto client = std::make_shared<net::Socket>(listener_.accept());
    if (!running_ || !client->valid()) break;
    std::lock_guard<std::mutex> lock(m_);
    clients_.push_back(client);
    workers_.emplace_back([this, client] { serve(client); });
  }
}

void ZoneController::serve(std::shared_ptr<net::Socket> client) {
  std::string line;
  int my_train = 0;
  while (client->read_line(line)) {
    if (line.rfind("BYE", 0) == 0) {  // train reached the terminus and leaves the zone
      std::lock_guard<std::mutex> lock(m_);
      positions_.erase(my_train);
      break;
    }
    auto report = parse_position(line);
    if (!report) {
      std::lock_guard<std::mutex> lock(m_);
      ++stats_.bad_messages;
      continue;
    }
    Authority ma{report->train, report->seq, 0};
    {
      std::lock_guard<std::mutex> lock(m_);
      if (!positions_.count(report->train)) ++stats_.trains_seen;
      my_train = report->train;
      positions_[report->train] = report->pos;
      ++stats_.reports;
      for (const auto& [id, pos] : positions_) {  // safety check against the latest reported positions
        if (id != report->train && pos > report->pos) {
          stats_.min_gap_m = std::min(stats_.min_gap_m, pos - tp_.length - report->pos);
        }
      }
      ma.limit = movement_authority(report->train, positions_, tp_, lp_);
    }
    if (!client->send_all(encode(ma))) break;
  }
}

void ZoneController::stop() {
  if (!running_.exchange(false)) return;
  {  // wake the accept() call with a throwaway connection; works the same on Windows and POSIX
    try {
      net::Socket wake = net::Socket::connect_loopback(port_);
    } catch (...) {
    }
  }
  if (acceptor_.joinable()) acceptor_.join();
  std::vector<std::thread> workers;
  {
    std::lock_guard<std::mutex> lock(m_);
    for (auto& c : clients_) c->shutdown_both();
    workers.swap(workers_);
  }
  for (auto& w : workers) w.join();
  listener_.close();
}

ControllerStats ZoneController::stats() const {
  std::lock_guard<std::mutex> lock(m_);
  return stats_;
}

}  // namespace cbtc
