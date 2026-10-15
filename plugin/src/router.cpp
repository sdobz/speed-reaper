// router.cpp — wheel laws (Scrl scroll, Shtl shuttle).
#include "router.h"

#include "jog_engine.h"  // JogEngine::kSecondsPerWheelUnit: Scrl scroll scale
#include "shuttle.h"  // ShuttleRate: SHTL law

Router::Router(IActionSink &sink, WheelState &wheel)
    : sink_(sink), wheel_(wheel), seen_changes_(wheel.changes) {}

void Router::OnWheel(std::int32_t delta) {
  if (wheel_.mode != WheelMode::Scrl || delta == 0) return;
  // Same timeline scale as the jog cursor (JogEngine): the view scrolls the
  // same seconds the cursor would move → same on-screen speed at any zoom.
  sink_.ScrollBySeconds(static_cast<double>(delta) *
                        JogEngine::kSecondsPerWheelUnit);
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
