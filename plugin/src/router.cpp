// router.cpp — wheel laws (Scrl scroll, Shtl shuttle).
#include "router.h"

#include "jog_engine.h"  // IZoomSource, kUnitsPerDetent: shared Scrl scale
#include "shuttle.h"  // ShuttleRate: SHTL law

Router::Router(IActionSink &sink, WheelState &wheel, IZoomSource &zoom,
               double pixels_per_detent)
    : sink_(sink),
      wheel_(wheel),
      zoom_(zoom),
      pixels_per_detent_(pixels_per_detent),
      seen_changes_(wheel.changes) {}

void Router::OnWheel(std::int32_t delta) {
  if (wheel_.mode != WheelMode::Scrl || delta == 0) return;
  // Same screen-space scale as the jog cursor (JogEngine::Tick, 46 §3): one
  // detent scrolls k_px screen pixels at any zoom. Degenerate zoom
  // (pxPerSec <= 0) → no scroll.
  const double px_per_sec = zoom_.PixelsPerSecond();
  if (px_per_sec <= 0.0) return;
  sink_.ScrollBySeconds((delta / JogEngine::kUnitsPerDetent) *
                        (pixels_per_detent_ / px_per_sec));
}

void Router::OnShuttleOffset(std::int32_t offset) {
  if (wheel_.mode != WheelMode::Shtl) return;  // no-op outside SHTL
  if (wheel_.changes != seen_changes_) {  // any mode change re-arms the stop
    seen_changes_ = wheel_.changes;
    deadband_stop_ = false;
  }
  const double rate = ShuttleRate(offset);
  if (rate == 0.0) {
    // Dead band: pause (no rewind). One HardStop per band entry; re-armed
    // when the wheel leaves the band (below) or on any mode change.
    if (!deadband_stop_) {
      deadband_stop_ = true;
      sink_.HardStop();
    }
    return;
  }
  deadband_stop_ = false;
  sink_.ApplyShuttleRate(rate);  // absolute law: re-applied every tick
}
