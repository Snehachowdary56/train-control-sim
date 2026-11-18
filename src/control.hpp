// Train control logic with no I/O, so it can be unit tested on its own.
// Model: a single-track line, trains run in one direction, positions in metres from the line start.
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>

namespace cbtc {

struct TrainParams {
  double max_speed = 22.0;      // m/s (about 80 km/h)
  double accel = 0.8;           // m/s^2
  double service_brake = 1.0;   // m/s^2
  double emergency_brake = 1.3; // m/s^2
  double reaction_s = 0.5;      // time before brakes take effect
  double length = 120.0;        // m
};

struct LineParams {
  double length = 10000.0;          // m
  double station_spacing = 1000.0;  // a station every km
  double dwell_s = 20.0;            // stop time at each station
  double safety_margin = 50.0;      // m kept clear behind the train ahead's rear
  double ma_timeout_s = 2.0;        // no fresh movement authority for this long = emergency brake
};

enum class Mode { Accel, Coast, Brake, Dwell, Emergency, Done };

inline const char* to_string(Mode m) {
  switch (m) {
    case Mode::Accel: return "ACCEL";
    case Mode::Coast: return "COAST";
    case Mode::Brake: return "BRAKE";
    case Mode::Dwell: return "DWELL";
    case Mode::Emergency: return "EB";
    case Mode::Done: return "DONE";
  }
  return "?";
}

// Distance needed to stop from speed v, including the distance covered during the reaction time.
inline double stopping_distance(double v, double brake, double reaction_s) {
  return v * reaction_s + (v * v) / (2.0 * brake);
}

// Movement authority: how far a train may go. Up to the rear of the train ahead minus a safety margin,
// otherwise to the end of the line. `positions` holds the latest reported front position of every train.
inline double movement_authority(int train_id, const std::map<int, double>& positions, const TrainParams& tp,
                                 const LineParams& lp) {
  const double me = positions.at(train_id);
  double limit = lp.length;
  for (const auto& [id, pos] : positions) {
    if (id != train_id && pos > me) limit = std::min(limit, pos - tp.length - lp.safety_margin);
  }
  return std::max(limit, me);  // never authorises going backwards
}

struct TrainState {
  double t = 0, pos = 0, speed = 0;
  double ma = 0;              // latest movement authority received
  double ma_received_at = 0;  // sim time it was received
  double next_station = 0;
  double dwell_until = -1;
  Mode mode = Mode::Coast;
  bool ma_violation = false;  // front passed the authority limit (must never happen)
};

// One control step of length dt. Picks the nearest target (authority limit or next station stop),
// brakes when the stopping distance reaches it, otherwise accelerates up to line speed.
inline void step(TrainState& s, double dt, const TrainParams& tp, const LineParams& lp) {
  if (s.mode == Mode::Done) return;
  s.t += dt;
  if (s.mode == Mode::Dwell) {
    if (s.t >= s.dwell_until) {
      s.next_station += lp.station_spacing;
      s.mode = Mode::Coast;
    }
    return;
  }
  const bool ma_stale = (s.t - s.ma_received_at) > lp.ma_timeout_s;
  if (ma_stale || s.mode == Mode::Emergency) {
    s.mode = Mode::Emergency;  // stays latched until the train has stopped and a fresh MA arrives
    s.speed = std::max(0.0, s.speed - tp.emergency_brake * dt);
    s.pos += s.speed * dt;
    if (s.speed == 0.0 && !ma_stale) s.mode = Mode::Coast;
    if (s.pos > s.ma + 1e-6) s.ma_violation = true;
    return;
  }
  const double target = std::min({s.ma, s.next_station, lp.length});
  const double gap = target - s.pos;
  // look one step ahead so the discrete simulation never overshoots the target
  const double needed = stopping_distance(s.speed, tp.service_brake, tp.reaction_s) + s.speed * dt + 0.5;
  if (gap <= needed) {
    s.mode = Mode::Brake;
    s.speed = std::max(0.0, s.speed - tp.service_brake * dt);
    if (gap < 1.0 && s.speed < 0.3) s.speed = 0.0;
  } else if (s.speed < tp.max_speed) {
    s.mode = Mode::Accel;
    s.speed = std::min(tp.max_speed, s.speed + tp.accel * dt);
  } else {
    s.mode = Mode::Coast;
  }
  s.pos = std::min(s.pos + s.speed * dt, std::max(s.pos, target));  // a stopped train creeps no further
  if (s.pos > s.ma + 1e-6) s.ma_violation = true;
  if (s.speed == 0.0 && target - s.pos < 1.0) {
    if (target >= lp.length - 1.0) {
      s.mode = Mode::Done;
    } else if (s.next_station - s.pos < 1.0) {
      s.mode = Mode::Dwell;
      s.dwell_until = s.t + lp.dwell_s;
    }
  }
}

}  // namespace cbtc
