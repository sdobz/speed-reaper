// nudge.h — pure hold+rotate nudge math (M7, design §4).
// Pure: no REAPER API, no HID. Each verb function takes the item geometry
// before the step, a signed number of steps (from HoldNudge::Add), and the
// step size in seconds (1/Fps()), and returns the geometry after the steps.
// Edge clamps: item length stays >= one step, take start offset stays >= 0.
#ifndef SPEED_REAPER_PLUGIN_NUDGE_H
#define SPEED_REAPER_PLUGIN_NUDGE_H

#include "project.h"

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
