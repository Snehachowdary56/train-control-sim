#include <cmath>
#include <map>

#include "check.hpp"
#include "control.hpp"

using namespace cbtc;

static void stopping_distance_matches_physics() {
  // 20 m/s, 1 m/s^2, 0.5 s reaction: 10 m + 200 m
  CHECK(std::fabs(stopping_distance(20, 1.0, 0.5) - 210.0) < 1e-9);
  CHECK(stopping_distance(0, 1.0, 0.5) == 0.0);
}

static void authority_stops_behind_train_ahead() {
  TrainParams tp;
  LineParams lp;
  std::map<int, double> pos{{1, 3000}, {2, 1000}, {3, 200}};
  CHECK(std::fabs(movement_authority(2, pos, tp, lp) - (3000 - 120 - 50)) < 1e-9);
  CHECK(std::fabs(movement_authority(3, pos, tp, lp) - (1000 - 120 - 50)) < 1e-9);
  CHECK(movement_authority(1, pos, tp, lp) == lp.length);  // lead train: end of line
}

static void authority_never_goes_backwards() {
  TrainParams tp;
  LineParams lp;
  std::map<int, double> pos{{1, 1100}, {2, 1000}};  // closer than length + margin
  CHECK(movement_authority(2, pos, tp, lp) == 1000);
}

static void train_never_passes_its_authority() {
  TrainParams tp;
  LineParams lp;
  lp.station_spacing = 1e9;  // no stations, only the authority limit
  TrainState s;
  s.ma = 800;
  s.next_station = 1e9;
  for (int i = 0; i < 3000; ++i) {
    s.ma_received_at = s.t;  // keep authority fresh
    step(s, 0.1, tp, lp);
  }
  CHECK(!s.ma_violation);
  CHECK(s.speed == 0.0);
  CHECK(s.pos <= 800 && s.pos > 790);
}

static void stops_and_dwells_at_station() {
  TrainParams tp;
  LineParams lp;
  TrainState s;
  s.ma = lp.length;
  s.next_station = 1000;
  bool dwelled = false;
  double dwell_pos = 0;
  for (int i = 0; i < 2000; ++i) {
    s.ma_received_at = s.t;
    step(s, 0.1, tp, lp);
    if (s.mode == Mode::Dwell && !dwelled) {
      dwelled = true;
      dwell_pos = s.pos;
    }
  }
  CHECK(dwelled);
  CHECK(std::fabs(dwell_pos - 1000) < 1.0);
  CHECK(s.next_station >= 2000);  // left the station afterwards
}

static void stale_authority_triggers_emergency_brake_then_recovers() {
  TrainParams tp;
  LineParams lp;
  TrainState s;
  s.ma = 5000;
  s.next_station = 1e9;
  s.speed = 20;
  s.mode = Mode::Coast;
  for (int i = 0; i < 25; ++i) step(s, 0.1, tp, lp);  // 2.5 s with no new authority
  CHECK(s.mode == Mode::Emergency);
  const double v = s.speed;
  step(s, 0.1, tp, lp);
  CHECK(std::fabs((v - s.speed) - tp.emergency_brake * 0.1) < 1e-9);  // braking at the emergency rate
  for (int i = 0; i < 300 && s.speed > 0; ++i) step(s, 0.1, tp, lp);
  CHECK(s.speed == 0.0);
  s.ma_received_at = s.t;  // radio back
  step(s, 0.1, tp, lp);
  CHECK(s.mode != Mode::Emergency);
}

static void reaches_line_speed_but_not_more() {
  TrainParams tp;
  LineParams lp;
  TrainState s;
  s.ma = lp.length;
  s.next_station = 5000;
  double vmax = 0;
  for (int i = 0; i < 1000; ++i) {
    s.ma_received_at = s.t;
    step(s, 0.1, tp, lp);
    vmax = std::max(vmax, s.speed);
  }
  CHECK(std::fabs(vmax - tp.max_speed) < 1e-9);
}

int main() {
  stopping_distance_matches_physics();
  authority_stops_behind_train_ahead();
  authority_never_goes_backwards();
  train_never_passes_its_authority();
  stops_and_dwells_at_station();
  stale_authority_triggers_emergency_brake_then_recovers();
  reaches_line_speed_but_not_more();
  TEST_MAIN_END("test_control");
}
