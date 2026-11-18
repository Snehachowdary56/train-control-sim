// Text messages between trains and the zone controller, one per line:
//   train -> controller:  POS <train> <seq> <position_m> <speed_mps>
//   controller -> train:  MA <train> <seq> <limit_m>
#pragma once
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>

namespace cbtc {

struct PositionReport { int train = 0; long seq = 0; double pos = 0, speed = 0; };
struct Authority { int train = 0; long seq = 0; double limit = 0; };

inline std::string encode(const PositionReport& p) {
  char buf[128];
  std::snprintf(buf, sizeof buf, "POS %d %ld %.3f %.3f\n", p.train, p.seq, p.pos, p.speed);
  return buf;
}
inline std::string encode(const Authority& a) {
  char buf[96];
  std::snprintf(buf, sizeof buf, "MA %d %ld %.3f\n", a.train, a.seq, a.limit);
  return buf;
}

inline std::optional<PositionReport> parse_position(const std::string& line) {
  std::istringstream in(line);
  std::string tag;
  PositionReport p;
  if (!(in >> tag >> p.train >> p.seq >> p.pos >> p.speed) || tag != "POS" || p.train <= 0 || p.pos < 0 || p.speed < 0)
    return std::nullopt;
  return p;
}
inline std::optional<Authority> parse_authority(const std::string& line) {
  std::istringstream in(line);
  std::string tag;
  Authority a;
  if (!(in >> tag >> a.train >> a.seq >> a.limit) || tag != "MA") return std::nullopt;
  return a;
}

}  // namespace cbtc
