// hold_nudge.h — hold+rotate tick accumulator (M7, design §4).
// Pure: no REAPER API, no HID. The wheel sends ±1 tick per rotation step;
// a held verb advances one "step" (e.g. one frame at 1/Fps()) every
// ticks_per_step ticks, signed.
#ifndef SPEED_REAPER_PLUGIN_HOLD_NUDGE_H
#define SPEED_REAPER_PLUGIN_HOLD_NUDGE_H

#include <cstdint>

// Accumulates signed wheel ticks into whole verb steps.
//
// The accumulator keeps the remainder between steps; a remainder of +3 with
// ticks_per_step=4 is +3, and a remainder of −3 is −3 — i.e. remainders are
// symmetric toward zero (truncating division, not floor). Reversing
// direction therefore neither loses nor invents steps: Add(+3); Add(−3)
// yields 0 steps in total.
class HoldNudge {
 public:
  explicit HoldNudge(int ticks_per_step = 4);

  // Adds a signed tick delta (e.g. ±1 per wheel step) and returns the whole
  // number of steps (signed) the cumulative remainder now completes; the
  // non-completed remainder stays in the accumulator.
  int Add(std::int32_t delta);

  // The leftover ticks, |remainder| < ticks_per_step, sign of the remainder.
  std::int32_t Remainder() const;

 private:
  std::int64_t ticks_per_step_;
  std::int64_t remainder_;
};

#endif  // SPEED_REAPER_PLUGIN_HOLD_NUDGE_H
