// cbtc_sim: runs a train control scenario and prints a summary.
// Example: cbtc_sim --trains 4 --duration 1800 --radio-loss 3:300:304 --log run.csv
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "net.hpp"
#include "simulation.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

static std::atomic<bool> g_stop{false};

#ifdef _WIN32
// Windows console control handler: Ctrl+C or closing the window stops the trains cleanly.
static BOOL WINAPI on_console_event(DWORD event) {
  if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT) {
    g_stop = true;
    return TRUE;
  }
  return FALSE;
}
#else
static void on_signal(int) { g_stop = true; }
#endif

static void usage() {
  std::cerr << "usage: cbtc_sim [--trains N] [--headway M] [--duration S] [--radio-loss TRAIN:FROM:TO]... [--log FILE]\n";
}

int main(int argc, char** argv) {
#ifdef _WIN32
  SetConsoleCtrlHandler(on_console_event, TRUE);
#else
  std::signal(SIGINT, on_signal);
#endif
  net::NetworkInit network;
  cbtc::Scenario sc;
  std::string log_path;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> std::string {
      if (i + 1 >= argc) { usage(); std::exit(2); }
      return argv[++i];
    };
    if (a == "--trains") sc.trains = std::stoi(next());
    else if (a == "--headway") sc.headway_m = std::stod(next());
    else if (a == "--duration") sc.duration_s = std::stod(next());
    else if (a == "--log") log_path = next();
    else if (a == "--radio-loss") {
      cbtc::RadioLoss l;
      if (std::sscanf(next().c_str(), "%d:%lf:%lf", &l.train, &l.from_s, &l.to_s) != 3) { usage(); return 2; }
      sc.radio_losses.push_back(l);
    } else { usage(); return 2; }
  }

  try {
    auto r = cbtc::run(sc, &g_stop);
    if (!log_path.empty()) cbtc::write_csv(r, log_path);
    std::printf("train  finish_s  max_kmh  authority_stops  emergency_brakes  authority_violation\n");
    bool ok = true;
    for (const auto& t : r.trains) {
      std::printf("%5d  %8.1f  %7.1f  %15d  %16d  %s\n", t.train, t.finish_s, t.max_speed * 3.6, t.authority_stops,
                  t.emergency_brakes, t.ma_violation ? "YES" : "no");
      ok = ok && !t.ma_violation;
    }
    std::printf("controller: %ld position reports, %d trains, %ld bad messages, closest approach %.1f m behind train ahead\n",
                r.controller.reports, r.controller.trains_seen, r.controller.bad_messages, r.controller.min_gap_m);
    std::printf("radio: %ld reports lost\n", r.messages_lost);
    return ok && r.controller.min_gap_m >= 0 ? 0 : 1;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
