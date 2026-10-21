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
// (delta / kUnitsPerDetent) × (k_px / pxPerSec) seconds — the same
// screen-space scale as the jog cursor (JogEngine), so the view travels the
// same on-screen distance (k_px pixels per detent) at any zoom (seconds, not
// lines). The live pxPerSec comes from the shared IZoomSource; a degenerate
// pxPerSec <= 0 scrolls nothing. Other modes ignore the delta.

#ifndef SPEED_REAPER_ROUTER_H
#define SPEED_REAPER_ROUTER_H

#include <cstdint>

#include "action_sink.h"
#include "jog_engine.h"  // IZoomSource, JogEngine::kUnitsPerDetent
#include "verb_registry.h"  // WheelState
#include "wheel_mode.h"

class Router {
 public:
  // `zoom` / `pixels_per_detent` (k_px): the shared screen-space scale the
  // Scrl scroll computes in (46 §3): same seam + k_px as JogEngine.
  Router(IActionSink &sink, WheelState &wheel, IZoomSource &zoom,
         double pixels_per_detent = 0.0);

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

  // Re-thread k_px (Session: after a bindings reload), matching
  // JogEngine::SetPixelsPerDetent.
  void SetPixelsPerDetent(double pixels_per_detent) {
    pixels_per_detent_ = pixels_per_detent;
  }

 private:
  IActionSink &sink_;
  WheelState &wheel_;
  IZoomSource &zoom_;
  double pixels_per_detent_ = 0.0;  // k_px: shared with JogEngine (46 §3)
  unsigned seen_changes_ = 0;  // wheel_.changes at the last shuttle tick
  bool deadband_stop_ = false; // Shtl: dead-band HardStop already issued
};

#endif  // SPEED_REAPER_ROUTER_H
