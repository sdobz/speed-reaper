// session.cpp — pure `Session`; see session.h.
#include "session.h"

#include <algorithm>

#include "led_state.h"
#include "project_ops.h"  // ReadLedState (pure op on IProject)
#include "trace.h"
#include "wheel_mode.h"

namespace {

// Engine step (ms): 125 Hz keeps the scrub motion smooth.
constexpr double kTimerMs = 8.0;
// Max catch-up per timer callback. REAPER can delay timer delivery by
// >200 ms while its main loop keeps running; consuming at most this much
// per callback bounds the cursor step inside one sample interval.
constexpr double kMaxCatchUpMs = 50.0;
// Don't spiral after a stall: the backlog itself is capped.
constexpr double kAccCapMs = 250.0;
// LED bar: poll the REAPER state every ~250 ms (time-based, not
// tick-count) and push the main-panel mask only when it changes.
constexpr std::int64_t kLedPollMs = 250;

const char *GestureKindName(GestureKind k) {
  switch (k) {
    case GestureKind::Press: return "press";
    case GestureKind::Tap: return "tap";
    case GestureKind::DoubleTap: return "double_tap";
    case GestureKind::HoldStart: return "hold_start";
    case GestureKind::HoldRotate: return "hold_rotate";
    case GestureKind::HoldEnd: return "hold_end";
  }
  return "?";
}

const char *WheelModeName(WheelMode m) {
  switch (m) {
    case WheelMode::Jog: return "Jog";
    case WheelMode::Scrl: return "Scrl";
    case WheelMode::Shtl: return "Shtl";
  }
  return "?";
}

}  // namespace

Session::Session(IDevice &device, IActionSink &actions, IProject &project,
                 IPlayheadSink &sink, std::string_view bindings_text)
    : device_(device),
      actions_(actions),
      project_(project),
      sink_(sink),
      verbs_(actions, project, bindings_text),
      gestures_(GestureConfig{verbs_.tuning().double_tap_ms,
                              verbs_.tuning().hold_ms}),
      router_(actions, verbs_.wheel_state()),
      dispatcher_(device, gestures_, router_),
      engine_(dispatcher_, sink) {
  ConfigureGestures();
  ReportErrors();
}

void Session::ConfigureGestures() {
  gestures_ = GestureEngine(GestureConfig{verbs_.tuning().double_tap_ms,
                                          verbs_.tuning().hold_ms});
  gestures_.SetDoubleTapKeys(verbs_.DoubleTapKeys());
  gestures_.SetPressKeys(verbs_.PressKeys());
}

void Session::ReportErrors() {
  for (const std::string &e : verbs_.errors()) {
    Trace("%s", e.c_str());
    actions_.Message((e + "\n").c_str());
  }
}

void Session::ReloadBindings(std::string_view bindings_text) {
  verbs_.Reload(bindings_text);
  ConfigureGestures();
  Trace("bindings loaded n=%d", verbs_.BindingCount());
  actions_.Message(("Speed Reaper: bindings loaded n=" +
                    std::to_string(verbs_.BindingCount()) + "\n").c_str());
  ReportErrors();
}

void Session::ShowBindings() {
  for (const std::string &l : verbs_.DescribeBindings())
    actions_.Message(l.c_str());
}

bool Session::ToggleEcho() {
  verbs_.SetEcho(!verbs_.Echo());
  Trace("echo %s", verbs_.Echo() ? "on" : "off");
  return verbs_.Echo();
}

void Session::ApplyModeToHid(WheelMode m) {
  Trace("mode %s", WheelModeName(m));
  device_.RequestWheelMode(ProtocolMode(m));
  const std::uint8_t jog_bits =
      (m == WheelMode::Jog ? 1u : m == WheelMode::Scrl ? 4u : 2u);
  Trace("led jog=%x main=%x", jog_bits, 0u);  // jog-mode LED only; the main-panel mask comes from the LED poll
  device_.RequestJogLeds(m == WheelMode::Jog, m == WheelMode::Shtl,
                         m == WheelMode::Scrl);
}

void Session::Tick(std::int64_t now_ms) {
  // Buttons: every key goes to the gesture engine; the bindings decide
  // what each (key, gesture) does.
  ButtonChange btn;
  while (device_.PopButton(&btn)) gestures_.OnKey(btn.key, btn.down, now_ms);
  gestures_.Poll(now_ms);  // every tick: advance tap-defer/hold timers
  Gesture gest;
  while (gestures_.Pop(&gest)) {
    Trace("gesture %04x %s", gest.key, GestureKindName(gest.kind));
    verbs_.OnGesture(gest);  // mapped → RunCommand (traces `cmd <id>`)
  }
  WheelMode changed;
  if (router_.TakeModeChange(&changed)) ApplyModeToHid(changed);
  // (Re)connect edge: re-send the current mode + jog LEDs, and force the
  // main-panel LEDs to be pushed again.
  const bool connected = device_.Connected();
  if (connected && !was_connected_) {
    ApplyModeToHid(router_.Mode());
    led_mask_ = -1;
  }
  was_connected_ = connected;
  // LED bar: poll the REAPER state every ~250 ms and push
  // the main-panel mask only when it changes or after a reconnect edge
  // (no point pushing while the device is not connected).
  if (connected && (led_mask_ < 0 || now_ms - last_led_ms_ >= kLedPollMs)) {
    last_led_ms_ = now_ms;
    const std::uint32_t mask = MainLedMask(ReadLedState(
        project_, verbs_.IsolateActive(), verbs_.SoloActive()));
    if (static_cast<std::uint32_t>(led_mask_) != mask) {
      led_mask_ = static_cast<int>(mask);
      Trace("led main=%x", mask);  // oracle greps
      device_.RequestLeds(mask);
    }
  }
  // Engine catch-up (fixed kTimerMs steps): REAPER fires the timer at
  // ~30 Hz, so consume wall-clock time in 8 ms steps, at most
  // kMaxCatchUpMs per callback, with the backlog capped at kAccCapMs.
  const double gap_ms =
      last_tick_ms_ < 0 ? 0.0 : static_cast<double>(now_ms - last_tick_ms_);
  if (gap_ms > 100.0) Trace("timer gap=%.0f", gap_ms);  // REAPER UI stall
  last_tick_ms_ = now_ms;
  acc_ms_ = std::min(acc_ms_ + gap_ms, kAccCapMs);
  if ((++calls_ % 100) == 1) Trace("timer call=%d", calls_);  // heartbeat
  double consumed_ms = 0.0;
  while (acc_ms_ >= kTimerMs && consumed_ms < kMaxCatchUpMs) {
    // The dispatcher routes each step's wheel input:
    // hold → gestures, else by mode Jog → engine / Scrl → view scroll /
    // Shtl → OnShuttleOffset(absolute) (SHTL law).
    engine_.Tick();
    acc_ms_ -= kTimerMs;
    consumed_ms += kTimerMs;
  }
}
