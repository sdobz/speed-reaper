// shuttle_test.cpp — ShuttleRate law (M4, design §8): dead band, edges,
// saturation, monotonicity in |offset|, odd symmetry, mid value.
#include "shuttle.h"

#include "check.h"

int main() {
  // Dead band: 0 and ±39 → 0.
  CHECK(ShuttleRate(0) == 0.0);
  CHECK(ShuttleRate(39) == 0.0);
  CHECK(ShuttleRate(-39) == 0.0);

  // Dead-band edge: ±40 → ±1.0.
  CHECK(ShuttleRate(40) == 1.0);
  CHECK(ShuttleRate(-40) == -1.0);

  // Saturation: ±4096 and beyond → ±8.0.
  CHECK(ShuttleRate(4096) == 8.0);
  CHECK(ShuttleRate(-4096) == -8.0);
  CHECK(ShuttleRate(5000) == 8.0);
  CHECK(ShuttleRate(-5000) == -8.0);

  // Monotonic (non-decreasing) in |offset| over 40..4096.
  bool monotonic = true;
  for (std::int32_t x = 40; x < 4096; ++x) {
    if (ShuttleRate(x + 1) < ShuttleRate(x)) monotonic = false;
  }
  CHECK(monotonic);

  // Odd symmetry: rate(−x) == −rate(x).
  bool symmetric = true;
  for (std::int32_t x = -5000; x <= 5000; x += 37) {
    if (ShuttleRate(-x) != -ShuttleRate(x)) symmetric = false;
  }
  CHECK(symmetric);

  // Mid value: offset 2068 → s = 0.5 → 1 + 7·0.5 = 4.5 (within 0.01).
  CHECK_NEAR(ShuttleRate(2068), 4.5, 0.01);
  CHECK_NEAR(ShuttleRate(-2068), -4.5, 0.01);

  return check_failures();
}
