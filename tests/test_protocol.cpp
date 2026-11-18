#include <cmath>

#include "check.hpp"
#include "protocol.hpp"

using namespace cbtc;

int main() {
  auto p = parse_position(encode(PositionReport{3, 42, 1234.5, 19.25}));
  CHECK(p && p->train == 3 && p->seq == 42 && std::fabs(p->pos - 1234.5) < 1e-6 && std::fabs(p->speed - 19.25) < 1e-6);
  auto a = parse_authority(encode(Authority{3, 42, 2830}));
  CHECK(a && a->train == 3 && a->seq == 42 && std::fabs(a->limit - 2830) < 1e-6);
  CHECK(!parse_position("HELLO"));
  CHECK(!parse_position("POS x 1 2 3"));
  CHECK(!parse_position("POS 1 1 -5 3"));   // negative position
  CHECK(!parse_position("POS 0 1 5 3"));    // invalid train id
  CHECK(!parse_authority("POS 1 1 5 3"));
  TEST_MAIN_END("test_protocol");
}
