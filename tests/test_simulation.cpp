// Integration tests: real TCP sockets on localhost, one thread per train plus the controller's threads.
#include "check.hpp"
#include "net.hpp"
#include "simulation.hpp"

using namespace cbtc;

static void four_trains_keep_separation_and_all_arrive() {
  Scenario sc;
  sc.trains = 4;
  auto r = run(sc);
  CHECK(r.controller.trains_seen == 4);
  CHECK(r.controller.bad_messages == 0);
  CHECK(r.controller.min_gap_m >= 0);
  int stops = 0;
  for (const auto& t : r.trains) {
    CHECK(!t.ma_violation);
    CHECK(t.finish_s > 0);
    CHECK(t.emergency_brakes == 0);
    stops += t.authority_stops;
  }
  CHECK(stops > 0);  // followers really were held back by the train ahead
}

static void radio_loss_causes_emergency_brake_on_that_train_only() {
  Scenario sc;
  sc.trains = 3;
  sc.radio_losses.push_back({2, 300, 305});
  auto r = run(sc);
  CHECK(r.messages_lost > 0);
  for (const auto& t : r.trains) {
    CHECK(!t.ma_violation);
    CHECK(t.emergency_brakes == (t.train == 2 ? 1 : 0));
    CHECK(t.finish_s > 0);  // recovers and still reaches the terminus
  }
}

static void many_trains_stress() {
  Scenario sc;
  sc.trains = 12;
  sc.headway_m = 300;
  sc.duration_s = 2400;
  auto r = run(sc);
  CHECK(r.controller.trains_seen == 12);
  CHECK(r.controller.min_gap_m >= 0);
  for (const auto& t : r.trains) CHECK(!t.ma_violation);
}

int main() {
  net::NetworkInit network;
  four_trains_keep_separation_and_all_arrive();
  radio_loss_causes_emergency_brake_on_that_train_only();
  many_trains_stress();
  TEST_MAIN_END("test_simulation");
}
