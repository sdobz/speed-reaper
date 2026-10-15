// session.h — pure `Session`: everything the REAPER timer callback does,
// in one testable object (40-cleanup §5). Owns the GestureEngine, Router,
// VerbRouter, JogEngine, the LED-mask state and the connect edge.
//
// Pure: all time is passed in via Tick(now_ms); no clock, no REAPER API,
// no HID. Input arrives through IDevice — implemented by HidWheelSource
// (the real device; its IWheelSource::Tick is the wheel tick) and by the
// test fake in plugin/test/session_test.cpp.

#ifndef SPEED_REAPER_SESSION_H
#define SPEED_REAPER_SESSION_H

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "action_sink.h"
#include "button_diff.h"
#include "gesture.h"
#include "jog_engine.h"
#include "project.h"
#include "router.h"
#include "verbs.h"
#include "wheel_dispatcher.h"

// The Speed Editor as seen by the Session: button/log queues, connected
// state, wheel tick (via IWheelDevice), and the mode/LED requests.
class IDevice : public IWheelDevice {
 public:
  ~IDevice() override = default;
  virtual bool PopButton(ButtonChange *out) = 0;   // false if empty
  virtual bool PopLog(std::string *out) = 0;       // false if empty
  virtual bool Connected() const = 0;
  virtual void RequestWheelMode(std::uint8_t protocolMode) = 0;
  virtual void RequestJogLeds(bool jog, bool shtl, bool scrl) = 0;
  virtual void RequestLeds(std::uint32_t mainMask) = 0;
};

class Session {
 public:
  Session(IDevice &device, IActionSink &actions, IProject &project,
          IPlayheadSink &sink);

  // One timer callback's worth of work (40-cleanup §5): button routing
  // (router keys → Router, everything else → gesture engine), gesture
  // poll → verbs, mode change → device, (re)connect re-send of mode +
  // LEDs, LED poll every ~250 ms (time-based, push on change), and
  // fixed-step engine catch-up (8 ms steps, ≤50 ms per call, backlog
  // capped at 250 ms).
  void Tick(std::int64_t now_ms);

 private:
  // Forward the mode to the device + the matching jog-mode LED (report
  // 0x04: bit 0 JOG, 1 SHTL, 2 SCRL).
  void ApplyModeToHid(WheelMode m);

  IDevice &device_;
  IProject &project_;
  IPlayheadSink &sink_;
  GestureEngine gestures_;  // tap / double-tap / hold recognition (design §7)
  Router router_;           // owns the wheel mode (Jog/Scrl/Shtl) (design §6)
  VerbRouter verbs_;        // gestures → verbs (design §7; M5 composites)
  WheelDispatcher dispatcher_;  // hold → gestures; else Jog/Scrl/Shtl (§6)
  JogEngine engine_;             // driven by the dispatcher (design §4)

  bool was_connected_ = false;  // false→true edge re-sends mode + LEDs
  int led_mask_ = -1;           // last pushed mask; -1 = not sent (force)
  std::int64_t last_led_ms_ = 0;  // last time-based LED poll
  double acc_ms_ = 0.0;      // engine catch-up backlog (≤ 250 ms)
  std::int64_t last_tick_ms_ = -1;
  int calls_ = 0;            // heartbeat counter for the oracle trace
};

#endif  // SPEED_REAPER_SESSION_H
