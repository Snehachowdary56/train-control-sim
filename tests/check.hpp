// Tiny test helper: counts failures instead of aborting, works the same in Debug and Release.
#pragma once
#include <iostream>

inline int g_failures = 0;
#define CHECK(cond)                                                                 \
  if (!(cond)) {                                                                    \
    ++g_failures;                                                                   \
    std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n";      \
  } else                                                                            \
    (void)0
#define TEST_MAIN_END(name)                                                         \
  if (g_failures) { std::cerr << g_failures << " failure(s)\n"; return 1; }         \
  std::cout << name << ": all checks passed\n";                                     \
  return 0
