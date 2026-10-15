// session.cpp — pure `Session` (40-cleanup §5); see session.h.
#include "session.h"

#include <algorithm>

#include "led_state.h"
#include "project_ops.h"  // ReadLedState (pure op on IProject, 40-cleanup §8)
#include "speed_editor/key_map.h"  // KEY_* codes for the router-key split
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
// LED bar (M8): poll the REAPER state every ~250 ms (time-based, not
// tick-count) and push the main-panel mask only when it changes.
constexpr std::int64_t kLedPollMs = 250;

const char *GestureKindName(GestureKind k) {
  switch (k) {
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
                 IPlayheadSink &sink)
    : device_(device),
      project_(project),
      sink_(sink),
      router_(actions),
      verbs_(actions, project),
      dispatcher_(device, gestures_, router_),
      engine_(dispatcher_, sink) {
  gestures_.SetDoubleTapKeys(VerbDoubleTapKeys());  // IN, OUT, ESC (design §7)
}

void Session::ApplyModeToHid(WheelMode m) {
  Trace("mode %s", WheelModeName(m));
  device_.RequestWheelMode(ProtocolMode(m));
  const std::uint8_t jog_bits =
      (m == WheelMode::Jog ? 1u : m == WheelMode::Scrl ? 4u : 2u);
  Trace("led jog=%x main=%x", jog_bits, 0u);  // no main-panel LEDs until M3
  device_.RequestJogLeds(m == WheelMode::Jog, m == WheelMode::Shtl,
                         m == WheelMode::Scrl);
}

void Session::Tick(std::int64_t now_ms) {
  // Buttons (design §6/§7): router keys (JOG/SCRL/SHTL/STOP-PLAY) go only
  // to the Router (press semantics live there); every other key goes only
  // to the gesture engine.
  ButtonChange btn;
  while (device_.PopButton(&btn)) {
    switch (btn.key) {
      case KEY_JOG:
      case KEY_SCRL:
      case KEY_SHTL:
      case KEY_STOP_PLAY:
        router_.OnButton(btn);
        break;
      default:
        gestures_.OnKey(btn.key, btn.down, now_ms);
        break;
    }
  }
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
  // LED bar (M8, design §5): poll the REAPER state every ~250 ms and push
  // the main-panel mask only when it changes or after a reconnect edge
  // (no point pushing while the device is not connected).
  if (connected && (led_mask_ < 0 || now_ms - last_led_ms_ >= kLedPollMs)) {
    last_led_ms_ = now_ms;
    const std::uint32_t mask = MainLedMask(ReadLedState(
        project_, verbs_.IsolateActive(), verbs_.SoloActive()));
    if (static_cast<std::uint32_t>(led_mask_) != mask) {
      led_mask_ = static_cast<int>(mask);
      Trace("led main=%x", mask);  // oracle greps (M3 edit verbs task 5)
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
    // The dispatcher (40-cleanup §6) routes each step's wheel input:
    // hold → gestures, else by mode Jog → engine / Scrl → view scroll /
    // Shtl → OnShuttleOffset(absolute) (M4 SHTL law).
    engine_.Tick();
    acc_ms_ -= kTimerMs;
    consumed_ms += kTimerMs;
  }
}
