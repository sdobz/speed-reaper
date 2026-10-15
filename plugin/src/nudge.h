// nudge.h — pure hold+rotate nudge math (M7, design §4).
// Pure: no REAPER API, no HID. HoldNudge turns wheel ticks into whole verb
// steps; each verb function takes the item geometry before the step, a signed
// number of steps (from HoldNudge::Add), and the step size in seconds
// (1/Fps()), and returns the geometry after the steps. Edge clamps: item
// length stays >= one step, take start offset stays >= 0.
#ifndef SPEED_REAPER_PLUGIN_NUDGE_H
#define SPEED_REAPER_PLUGIN_NUDGE_H

#include <cstdint>

#include "project.h"

// Accumulates signed wheel ticks into whole verb steps.
//
// The wheel sends ±1 tick per rotation step; a held verb advances one "step"
// (e.g. one frame at 1/Fps()) every ticks_per_step ticks, signed.
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

// SPLIT (MOVE): pos += steps * step.
ItemGeom NudgeMove(const ItemGeom &g, int steps, double step_seconds);

// TRIM IN: move the left edge by steps * step; content stays, so
// startoffs shifts by (applied step) * playrate. Len clamp: len >= step.
ItemGeom NudgeTrimIn(const ItemGeom &g, int steps, double step_seconds);

// TRIM OUT: move the right edge by steps * step (len only).
// Len clamp: len >= step.
ItemGeom NudgeTrimOut(const ItemGeom &g, int steps, double step_seconds);

// SLIP SRC: startoffs += (applied step) * playrate; pos and len fixed.
ItemGeom NudgeSlipSrc(const ItemGeom &g, int steps, double step_seconds);

// SLIP DEST: pos += steps * step and startoffs -= (applied step) * playrate
// so the content stays fixed in time.
ItemGeom NudgeSlipDest(const ItemGeom &g, int steps, double step_seconds);

// AUDIO LEVEL: vol in 0.5 dB per step, clamped to 0 … +12 dB (vol is linear,
// 1.0 = 0 dB). step_seconds is unused.
ItemGeom NudgeVolume(const ItemGeom &g, int steps, double step_seconds);

#endif  // SPEED_REAPER_PLUGIN_NUDGE_H
