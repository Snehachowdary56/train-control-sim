// Runs a whole scenario: starts the zone controller, launches one thread per train, and collects logs.
#pragma once
#include <atomic>
#include <string>
#include <vector>

#include "control.hpp"
#include "zone_controller.hpp"

namespace cbtc {

struct RadioLoss { int train = 0; double from_s = 0, to_s = 0; };

struct Scenario {
  int trains = 4;
  double headway_m = 450.0;     // starting distance between trains
  double duration_s = 1800.0;
  double dt = 0.1;
  int report_every_steps = 2;   // position report every 0.2 s
  std::vector<RadioLoss> radio_losses;
  TrainParams train;
  LineParams line;
};

struct LogRow { double t; int train; double pos, speed, ma; Mode mode; };

struct TrainSummary {
  int train = 0;
  double finish_s = -1;  // -1 = did not reach the terminus
  double max_speed = 0;
  int emergency_brakes = 0;
  int authority_stops = 0;   // times the train braked to a stop because of the train ahead
  bool ma_violation = false;
};

struct SimResult {
  std::vector<LogRow> log;
  std::vector<TrainSummary> trains;
  ControllerStats controller;
  long messages_lost = 0;
};

SimResult run(const Scenario& sc, const std::atomic<bool>* stop_flag = nullptr);
void write_csv(const SimResult& r, const std::string& path);

}  // namespace cbtc
