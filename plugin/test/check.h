// check.h — minimal unit-test helpers (no framework). A test file is a
// single .cpp with main(); use CHECK / CHECK_NEAR; return failures() from main.
#ifndef SPEED_REAPER_TEST_CHECK_H
#define SPEED_REAPER_TEST_CHECK_H
#include <cmath>
#include <cstdio>

inline int &check_failures() { static int n = 0; return n; }

#define CHECK(cond)                                                        \
  do {                                                                     \
    if (!(cond)) {                                                         \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      ++check_failures();                                                  \
    }                                                                      \
  } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

#endif
