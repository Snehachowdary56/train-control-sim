#include "simulation.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "barrier.hpp"
#include "protocol.hpp"

namespace cbtc {

namespace {

bool in_radio_loss(const Scenario& sc, int train, double t) {
  for (const auto& l : sc.radio_losses)
    if (l.train == train && t >= l.from_s && t < l.to_s) return true;
  return false;
}

struct TrainResult {
  std::vector<LogRow> log;
  TrainSummary summary;
  long lost = 0;
};

TrainResult run_train(int id, double start_pos, uint16_t port, const Scenario& sc, StepBarrier& barrier,
                      const std::atomic<bool>* stop_flag) {
  TrainResult out;
  out.summary.train = id;
  net::Socket sock = net::Socket::connect_loopback(port);
  sock.set_recv_timeout_ms(5000);
  TrainState s;
  s.pos = start_pos;
  s.ma = start_pos;
  s.next_station = (std::floor(start_pos / sc.line.station_spacing) + 1) * sc.line.station_spacing;
  long seq = 0;
  Mode prev = Mode::Coast;
  bool was_moving = false;
  const long steps = static_cast<long>(sc.duration_s / sc.dt);

  for (long i = 0; i < steps && s.mode != Mode::Done; ++i) {
    if (stop_flag && stop_flag->load()) break;
    if (i % sc.report_every_steps == 0) {
      if (in_radio_loss(sc, id, s.t)) {
        ++out.lost;
      } else {
        std::string line;
        if (!sock.send_all(encode(PositionReport{id, ++seq, s.pos, s.speed})) || !sock.read_line(line))
          throw std::runtime_error("train " + std::to_string(id) + " lost the zone controller");
        if (auto ma = parse_authority(line)) {
          s.ma = ma->limit;
          s.ma_received_at = s.t;
        }
      }
    }
    step(s, sc.dt, sc.train, sc.line);
    out.summary.max_speed = std::max(out.summary.max_speed, s.speed);
    if (s.mode == Mode::Emergency && prev != Mode::Emergency) ++out.summary.emergency_brakes;
    if (was_moving && s.speed == 0.0 && s.mode == Mode::Brake && s.ma < s.next_station - 1.0) ++out.summary.authority_stops;
    was_moving = s.speed > 0.0;
    if (i % 10 == 0 || s.mode != prev) out.log.push_back({s.t, id, s.pos, s.speed, s.ma, s.mode});
    prev = s.mode;
    barrier.arrive_and_wait();
  }
  if (s.mode == Mode::Done) {
    out.summary.finish_s = s.t;
    sock.send_all("BYE " + std::to_string(id) + "\n");
  }
  out.summary.ma_violation = s.ma_violation;
  barrier.arrive_and_drop();
  return out;
}

}  // namespace

SimResult run(const Scenario& sc, const std::atomic<bool>* stop_flag) {
  if (sc.trains < 1) throw std::invalid_argument("need at least one train");
  ZoneController zc(sc.train, sc.line);
  const uint16_t port = zc.start();
  StepBarrier barrier(static_cast<std::size_t>(sc.trains));
  std::vector<TrainResult> results(static_cast<std::size_t>(sc.trains));
  std::vector<std::thread> threads;
  std::mutex err_m;
  std::string error;
  for (int k = 0; k < sc.trains; ++k) {
    const int id = k + 1;                                   // train 1 is at the front
    const double start = (sc.trains - 1 - k) * sc.headway_m;
    threads.emplace_back([&, id, start, k] {
      try {
        results[static_cast<std::size_t>(k)] = run_train(id, start, port, sc, barrier, stop_flag);
      } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(err_m);
        error = e.what();
        barrier.arrive_and_drop();
      }
    });
  }
  for (auto& t : threads) t.join();
  zc.stop();
  if (!error.empty()) throw std::runtime_error(error);

  SimResult r;
  r.controller = zc.stats();
  for (auto& tr : results) {
    r.log.insert(r.log.end(), tr.log.begin(), tr.log.end());
    r.trains.push_back(tr.summary);
    r.messages_lost += tr.lost;
  }
  std::sort(r.log.begin(), r.log.end(), [](const LogRow& a, const LogRow& b) {
    return a.t != b.t ? a.t < b.t : a.train < b.train;
  });
  return r;
}

void write_csv(const SimResult& r, const std::string& path) {
  std::ofstream f(path);
  if (!f) throw std::runtime_error("cannot write " + path);
  f << "t_s,train,position_m,speed_mps,authority_m,mode\n";
  f.setf(std::ios::fixed);
  f.precision(2);
  for (const auto& row : r.log)
    f << row.t << ',' << row.train << ',' << row.pos << ',' << row.speed << ',' << row.ma << ',' << to_string(row.mode) << '\n';
}

}  // namespace cbtc
