// router.h — wheel laws (Scrl scroll, Shtl shuttle) over the shared wheel mode.
//
// Router translates wheel deltas into IActionSink calls. Unit-testable: no HID, no REAPER.
// deltas into IActionSink calls. Unit-testable: no HID, no REAPER.
//
// The wheel mode itself (Jog/Scrl/Shtl) lives in the shared WheelState
// (verb_registry.h): the `wheel` / `transport` verbs change it, the Router
// only reads it and applies the Scrl / Shtl wheel laws.
//
// OnWheel is only meaningful in Scrl mode: it scrolls the arrange view by
// delta × JogEngine::kSecondsPerWheelUnit seconds — the same timeline scale
// as the jog cursor, so the view travels the same on-screen distance per
// wheel unit at any zoom (seconds, not lines). Other modes ignore the
// delta.

#ifndef SPEED_REAPER_ROUTER_H
#define SPEED_REAPER_ROUTER_H

#include <cstdint>

#include "action_sink.h"
#include "verb_registry.h"  // WheelState
#include "wheel_mode.h"

class Router {
 public:
  Router(IActionSink &sink, WheelState &wheel);

  // Accumulate a wheel delta (Scrl mode only, see above).
  void OnWheel(std::int32_t delta);

  // SHTL law: the absolute wheel offset maps through ShuttleRate() to the
  // sink. Called
  // every engine tick (main.cpp); a no-op outside Shtl mode. rate 0
  // (dead band, |offset| < 40) → HardStop() once (no rewind; re-armed when
  // the wheel leaves the band or the mode changes). rate ≠ 0 →
  // ApplyShuttleRate(rate), re-applied every tick (absolute: no integration).
  void OnShuttleOffset(std::int32_t offset);

  WheelMode Mode() const { return wheel_.mode; }

  // True exactly once after each mode change (consumed); *m receives the
  // new mode. main.cpp forwards it via RequestWheelMode + RequestJogLeds.
  bool TakeModeChange(WheelMode *m) { return wheel_.TakeModeChange(m); }

 private:
  IActionSink &sink_;
  WheelState &wheel_;
  unsigned seen_changes_ = 0;  // wheel_.changes at the last shuttle tick
  bool deadband_stop_ = false; // Shtl: dead-band HardStop already issued
};

#endif  // SPEED_REAPER_ROUTER_H
