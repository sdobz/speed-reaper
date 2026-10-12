// router.cpp — press-semantics buttons + wheel mode routing (design §6).
#include "router.h"

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
      SetMode(WheelMode::Jog);
      break;
    case KEY_SCRL:
      SetMode(WheelMode::Scrl);
      break;
    case KEY_SHTL:
      break;  // ignored until M4: mode unchanged, no LED change
    case KEY_STOP_PLAY:
      sink_.RunCommand(kStopPlayCommand);
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
}
