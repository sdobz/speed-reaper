// session.h — pure `Session`: everything the REAPER timer callback does,
// in one testable object. Owns the GestureEngine, Router,
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
#include <string_view>
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
  // `bindings_text` = the bindings.ini contents; empty = the embedded
  // default. Bad lines are skipped and reported via actions.Message.
  Session(IDevice &device, IActionSink &actions, IProject &project,
          IPlayheadSink &sink, std::string_view bindings_text = {});

  // One timer callback's worth of work: button routing
  // (every key → gesture engine), gesture poll → bound verbs, mode change → device, (re)connect re-send of mode +
  // LEDs, LED poll every ~250 ms (time-based, push on change), and
  // fixed-step engine catch-up (8 ms steps, ≤50 ms per call, backlog
  // capped at 250 ms).
  void Tick(std::int64_t now_ms);

  // Re-read bindings (empty = embedded default); console:
  // "Speed Reaper: bindings loaded n=<count>" + each error line.
  void ReloadBindings(std::string_view bindings_text);
  // Print the live table to the console (grouped by key, then unbound).
  void ShowBindings();
  // Echo toggle: while on, each fired binding prints KEY.gesture → verb.
  bool ToggleEcho();
  bool EchoOn() const { return verbs_.Echo(); }

 private:
  // Forward the mode to the device + the matching jog-mode LED (report
  // 0x04: bit 0 JOG, 1 SHTL, 2 SCRL).
  void ApplyModeToHid(WheelMode m);
  void ConfigureGestures();  // tuning + press / double-tap sets
  void ReportErrors();

  IDevice &device_;
  IActionSink &actions_;
  IProject &project_;
  IPlayheadSink &sink_;
  VerbRouter verbs_;        // bindings table: (key, gesture) → verb
  GestureEngine gestures_;  // press / tap / double-tap / hold recognition
  Router router_;           // Scrl / Shtl wheel laws over verbs_' wheel mode
  WheelDispatcher dispatcher_;  // hold → gestures; else Jog/Scrl/Shtl
  JogEngine engine_;             // driven by the dispatcher

  bool was_connected_ = false;  // false→true edge re-sends mode + LEDs
  int led_mask_ = -1;           // last pushed mask; -1 = not sent (force)
  std::int64_t last_led_ms_ = 0;  // last time-based LED poll
  double acc_ms_ = 0.0;      // engine catch-up backlog (≤ 250 ms)
  std::int64_t last_tick_ms_ = -1;
  int calls_ = 0;            // heartbeat counter for the oracle trace
};

#endif  // SPEED_REAPER_SESSION_H
