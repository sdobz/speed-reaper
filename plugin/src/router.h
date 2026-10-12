// router.h — press-semantics buttons + wheel mode routing (design §6).
//
// Router owns the current WheelMode and translates button edges / wheel
// deltas into IActionSink calls. Unit-testable: no HID, no REAPER.
//
// Press rules (down edge only; releases ignored):
//   JOG       → mode = Jog
//   SCRL      → mode = Scrl
//   SHTL      → ignored until M4 (mode unchanged, no LED change)
//   STOP/PLAY → RunCommand(40044)
// Initial mode: Jog.
//
// OnWheel is only meaningful in Scrl mode: it accumulates wheel units and
// issues ScrollView(±1) once per kScrollUnitsPerStep units, keeping the
// remainder. Other modes ignore the delta.
//
// ModeGatedSource (design §6) wraps the HID source for the engine: in Jog
// the delta passes through (playhead motion); otherwise it is routed to
// Router::OnWheel and the engine sees 0, so SCRL never moves the playhead.

#ifndef SPEED_REAPER_ROUTER_H
#define SPEED_REAPER_ROUTER_H

#include <cstdint>

#include "action_sink.h"
#include "button_diff.h"
#include "jog_engine.h"  // IWheelSource, WheelSample
#include "wheel_mode.h"

class Router {
 public:
  // Wheel units (signed ticks) that accumulate into one ScrollView step.
  static constexpr std::int32_t kScrollUnitsPerStep = 20;

  explicit Router(IActionSink &sink);

  // One button edge from the worker (design §2). Down edges act;
  // releases are ignored. Unknown keys are ignored.
  void OnButton(const ButtonChange &change);

  // Accumulate a wheel delta (Scrl mode only, see above).
  void OnWheel(std::int32_t delta);

  WheelMode Mode() const { return mode_; }

  // True exactly once after each mode change (consumed); *m receives the
  // new mode. main.cpp forwards it via RequestWheelMode + RequestJogLeds.
  bool TakeModeChange(WheelMode *m);

 private:
  void SetMode(WheelMode m);

  IActionSink &sink_;
  WheelMode mode_ = WheelMode::Jog;
  bool mode_change_pending_ = false;
  std::int32_t scroll_remainder_ = 0;
};

// IWheelSource for the engine: passes the wrapped source's delta through in
// Jog, routes it to the Router (horizontal scroll) and outputs 0 otherwise.
class ModeGatedSource : public IWheelSource {
 public:
  ModeGatedSource(IWheelSource &hid, Router &r) : hid_(hid), router_(r) {}

  void Tick(WheelSample *out) override {
    WheelSample s;
    hid_.Tick(&s);
    if (router_.Mode() == WheelMode::Jog) {
      *out = s;
    } else {
      router_.OnWheel(s.delta);
      out->delta = 0;
    }
  }

 private:
  IWheelSource &hid_;
  Router &router_;
};

#endif  // SPEED_REAPER_ROUTER_H
