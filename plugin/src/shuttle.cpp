// shuttle.cpp — see shuttle.h.
#include "shuttle.h"

#include <algorithm>
#include <cmath>

double ShuttleRate(std::int32_t offset) {
  const double abs = std::fabs(static_cast<double>(offset));
  if (abs < 40.0) return 0.0;
  const double s =
      std::clamp((abs - 40.0) / (4096.0 - 40.0), 0.0, 1.0);
  const double rate = 1.0 + 7.0 * s * s * (3.0 - 2.0 * s);
  return offset > 0 ? rate : -rate;
}
