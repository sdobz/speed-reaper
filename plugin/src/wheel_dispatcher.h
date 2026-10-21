// wheel_dispatcher.h — one pure class decides where each tick's wheel input
// goes. Replaces the old hold/mode decorator chain. Priority order, per
// engine tick:
//   active turn-hold → GestureEngine::OnWheel (a held *turn* key owns the
//     wheel; a tap-only key held while coasting leaves the delta on the jog
//     path, so its release stays a Tap)
//   else by mode:
//     Jog  → pass the delta through (JogEngine: playhead moves 1:1)
//     Scrl → Router::OnWheel (view scroll; engine sees 0)
//     Shtl → Router::OnShuttleOffset(absolute) (SHTL law)
//
// Pure: the device is injected (IWheelDevice, satisfied by HidWheelSource
// and the test fakes); no HID, no REAPER, no clock.

#ifndef SPEED_REAPER_WHEEL_DISPATCHER_H
#define SPEED_REAPER_WHEEL_DISPATCHER_H

#include <cstdint>

#include "gesture.h"
#include "jog_engine.h"
#include "router.h"
#include "wheel_mode.h"

// The wheel side of the device as seen by the dispatcher: relative ticks
// plus the absolute offset (Shtl).
class IWheelDevice : public IWheelSource {
 public:
  ~IWheelDevice() override = default;
  virtual std::int32_t AbsolutePosition() const = 0;
};

class WheelDispatcher final : public IWheelSource {
 public:
  WheelDispatcher(IWheelDevice &device, GestureEngine &gestures, Router &router)
      : device_(device), gestures_(gestures), router_(router) {}

  // One engine tick: read the wheel sample and decide its destination.
  // *out carries the delta the JogEngine should apply (0 in every branch
  // except Jog).
  void Tick(WheelSample *out) override {
    WheelSample s;
    device_.Tick(&s);
    if (gestures_.TurnHoldActive()) {
      // A held turn key beats every mode: the wheel drives the held key's
      // HoldRotate; the jog/scroll/shuttle paths see nothing. A tap-only key
      // is not a turn key, so a coasting wheel still jogs while it is held.
      gestures_.OnWheel(s.delta, 0);  // now_ms is unused by OnWheel
      s.delta = 0;
    } else if (router_.Mode() == WheelMode::Jog) {
      // Jog: the delta passes through to the JogEngine (playhead motion).
    } else if (router_.Mode() == WheelMode::Scrl) {
      // Scrl: the view scrolls by delta × kSecondsPerWheelUnit; the
      // engine sees 0, so SCRL never moves the playhead.
      router_.OnWheel(s.delta);
      s.delta = 0;
    } else {  // WheelMode::Shtl
      // SHTL law: absolute offset → ShuttleRate → sink, re-applied
      // every tick (absolute: no integration). No playhead motion.
      router_.OnShuttleOffset(device_.AbsolutePosition());
      s.delta = 0;
    }
    *out = s;
  }

 private:
  IWheelDevice &device_;
  GestureEngine &gestures_;
  Router &router_;
};

#endif  // SPEED_REAPER_WHEEL_DISPATCHER_H
