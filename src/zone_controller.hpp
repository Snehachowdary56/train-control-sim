// Zone controller (wayside): one thread accepts connections and one thread serves each train.
// All trains' positions live in one map guarded by a mutex; every position report is answered with
// a movement authority computed from that shared view.
#pragma once
#include <atomic>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "control.hpp"
#include "net.hpp"

namespace cbtc {

struct ControllerStats {
  long reports = 0;
  long bad_messages = 0;
  double min_gap_m = std::numeric_limits<double>::infinity();  // closest any train got to the rear of the train ahead
  int trains_seen = 0;
};

class ZoneController {
 public:
  ZoneController(TrainParams tp, LineParams lp);
  ~ZoneController();
  uint16_t start();  // returns the port it listens on
  void stop();
  ControllerStats stats() const;

 private:
  void accept_loop();
  void serve(std::shared_ptr<net::Socket> client);

  TrainParams tp_;
  LineParams lp_;
  net::Socket listener_;
  uint16_t port_ = 0;
  std::atomic<bool> running_{false};
  std::thread acceptor_;
  mutable std::mutex m_;  // guards everything below
  std::map<int, double> positions_;
  std::vector<std::thread> workers_;
  std::vector<std::shared_ptr<net::Socket>> clients_;
  ControllerStats stats_;
};

}  // namespace cbtc
