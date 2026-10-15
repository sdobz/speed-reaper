// router.cpp — press-semantics buttons + wheel mode routing.
#include "router.h"

#include "jog_engine.h"  // JogEngine::kSecondsPerWheelUnit: Scrl scroll scale
#include "shuttle.h"  // ShuttleRate: SHTL law
#include "speed_editor/key_map.h"  // KEY_JOG, KEY_SCRL, KEY_SHTL, KEY_STOP_PLAY

namespace {
// REAPER command 40044: toggle play/stop (transport).
constexpr int kStopPlayCommand = 40044;
}  // namespace

Router::Router(IActionSink &sink) : sink_(sink) {}

void Router::OnButton(const ButtonChange &change) {
  if (!change.down) return;  // press semantics: down edge only
  switch (change.key) {
    case KEY_JOG:
    case KEY_SCRL: {
      // JOG/SCRL tap while shuttling hands off: rate back to 1.0, wheel back
      // to relative mode (docs/plugin-design-m4-shuttle-impl.md).
      if (mode_ == WheelMode::Shtl) sink_.ApplyShuttleRate(1.0);
      SetMode(change.key == KEY_JOG ? WheelMode::Jog : WheelMode::Scrl);
      break;
    }
    case KEY_SHTL:
      if (mode_ != WheelMode::Shtl) shuttle_prev_ = mode_;
      SetMode(WheelMode::Shtl);
      break;
    case KEY_STOP_PLAY:
      if (mode_ == WheelMode::Shtl) {
        // SHTL exit: hard stop (OnStopButton in REAPER, never 40045) + rate
        // 1.0, back to the prior mode (initial Jog).
        sink_.HardStop();
        SetMode(shuttle_prev_);
      } else {
        sink_.RunCommand(kStopPlayCommand);
      }
      break;
    default:
      break;
  }
}

void Router::OnWheel(std::int32_t delta) {
  if (mode_ != WheelMode::Scrl || delta == 0) return;
  // Same timeline scale as the jog cursor (JogEngine): the view scrolls the
  // same seconds the cursor would move → same on-screen speed at any zoom.
  sink_.ScrollBySeconds(static_cast<double>(delta) *
                        JogEngine::kSecondsPerWheelUnit);
}

void Router::OnShuttleOffset(std::int32_t offset) {
  if (mode_ != WheelMode::Shtl) return;  // no-op outside SHTL
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

bool Router::TakeModeChange(WheelMode *m) {
  if (!mode_change_pending_) return false;
  mode_change_pending_ = false;
  if (m != nullptr) *m = mode_;
  return true;
}

void Router::SetMode(WheelMode m) {
  if (m == mode_) return;  // re-pressing the active mode is not a change
  mode_ = m;
  mode_change_pending_ = true;
  deadband_stop_ = false;  // any mode change re-arms the dead-band stop
}
