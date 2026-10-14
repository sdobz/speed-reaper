// hold_nudge.cpp — hold+rotate tick accumulator (M7, design §4).
#include "hold_nudge.h"

HoldNudge::HoldNudge(int ticks_per_step)
    : ticks_per_step_(ticks_per_step > 0 ? ticks_per_step : 1), remainder_(0) {}

int HoldNudge::Add(std::int32_t delta) {
  remainder_ += delta;
  // Truncating division keeps remainders symmetric toward zero:
  // 10 = 4·2 + 2 and −10 = −4·2 + (−2).
  const std::int64_t steps = remainder_ / ticks_per_step_;
  remainder_ -= steps * ticks_per_step_;
  return static_cast<int>(steps);
}

std::int32_t HoldNudge::Remainder() const {
  return static_cast<std::int32_t>(remainder_);
}
