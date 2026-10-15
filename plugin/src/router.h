// router.h — press-semantics buttons + wheel mode routing (design §6).
//
// Router owns the current WheelMode and translates button edges / wheel
// deltas into IActionSink calls. Unit-testable: no HID, no REAPER.
//
// Press rules (down edge only; releases ignored):
//   JOG       → mode = Jog (from Shtl: also ApplyShuttleRate(1.0) hand-off)
//   SCRL      → mode = Scrl (from Shtl: also ApplyShuttleRate(1.0) hand-off)
//   SHTL      → mode = Shtl (M4; remembers the prior mode for STOP/PLAY exit)
//   STOP/PLAY → RunCommand(40044); in Shtl: HardStop() + exit to the prior mode
// Initial mode: Jog.
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
#include "button_diff.h"
#include "wheel_mode.h"

class Router {
 public:
  explicit Router(IActionSink &sink);

  // One button edge from the worker (design §2). Down edges act;
  // releases are ignored. Unknown keys are ignored.
  void OnButton(const ButtonChange &change);

  // Accumulate a wheel delta (Scrl mode only, see above).
  void OnWheel(std::int32_t delta);

  // SHTL law (M4, design §8, docs/plugin-design-m4-shuttle-impl.md): the
  // absolute wheel offset maps through ShuttleRate() to the sink. Called
  // every engine tick (main.cpp); a no-op outside Shtl mode. rate 0
  // (dead band, |offset| < 40) → HardStop() once (no rewind; re-armed when
  // the wheel leaves the band or the mode changes). rate ≠ 0 →
  // ApplyShuttleRate(rate), re-applied every tick (absolute: no integration).
  void OnShuttleOffset(std::int32_t offset);

  WheelMode Mode() const { return mode_; }

  // True exactly once after each mode change (consumed); *m receives the
  // new mode. main.cpp forwards it via RequestWheelMode + RequestJogLeds.
  bool TakeModeChange(WheelMode *m);

 private:
  void SetMode(WheelMode m);

  IActionSink &sink_;
  WheelMode mode_ = WheelMode::Jog;
  bool mode_change_pending_ = false;
  WheelMode shuttle_prev_ = WheelMode::Jog; // mode to return to on SHTL exit
  bool deadband_stop_ = false; // Shtl: dead-band HardStop already issued
};

#endif  // SPEED_REAPER_ROUTER_H
