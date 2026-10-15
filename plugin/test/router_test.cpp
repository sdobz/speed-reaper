// Characterizes Router (docs/archive/plugin-design-m1-m3.md §6, design §8 /
// docs/archive/plugin-design-m4-shuttle-impl.md for SHTL): press rules, STOP/PLAY,
// Scrl seconds-based view scrolling (same scale as the jog cursor), and
// SHTL shuttle-rate wiring. Wheel routing lives in WheelDispatcher
// (wheel_dispatcher_test.cpp, 40-cleanup §6). No REAPER, no HID.
#include "router.h"

#include "check.h"
#include "jog_engine.h"  // JogEngine::kSecondsPerWheelUnit: Scrl scroll scale
#include "shuttle.h"  // ShuttleRate: the law the Router must forward
#include "speed_editor/key_map.h"  // KEY_JOG, KEY_SCRL, KEY_SHTL, KEY_STOP_PLAY

namespace {

class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override {
    ++command_count;
    last_command = cmd;
  }
  void ScrollBySeconds(double dt) override {
    ++scroll_calls;
    scroll_dt_sum += dt;
  }
  void ApplyShuttleRate(double rate) override {
    ++rate_calls;
    last_rate = rate;
  }
  void HardStop() override { ++hard_stops; }
  void Message(const char *msg) override { (void)msg; }

  int command_count = 0;
  int last_command = 0;
  int scroll_calls = 0;
  double scroll_dt_sum = 0.0;
  int rate_calls = 0;
  double last_rate = 0.0;
  int hard_stops = 0;
};


}  // namespace

int main() {
  // Initial mode is Jog; nothing pending, nothing sent to the sink.
  {
    FakeSink sink;
    WheelState ws;
    Router r(sink, ws);
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));
    CHECK(sink.command_count == 0 && sink.scroll_calls == 0);
    CHECK(ProtocolMode(WheelMode::Jog) == 0);
    CHECK(ProtocolMode(WheelMode::Scrl) == 2);
    CHECK(ProtocolMode(WheelMode::Shtl) == 3);
  }

  // SHTL law (design §8): OnShuttleOffset feeds ShuttleRate to the sink.
  // Dead band (offset 0) → one HardStop, no rate call, no rewind; leaving
  // the band and re-entering re-arms the stop. ±2068 → ±4.5.
  {
    FakeSink sink;
    WheelState ws;
    Router r(sink, ws);
    ws.Set(WheelMode::Shtl);
    r.TakeModeChange(nullptr);

    r.OnShuttleOffset(0);
    CHECK(sink.rate_calls == 0);  // dead band: no rate call
    CHECK(sink.hard_stops == 1);  // pause
    r.OnShuttleOffset(0);
    CHECK(sink.hard_stops == 1);  // parked: no re-issue

    r.OnShuttleOffset(2068);
    CHECK(sink.rate_calls == 1);
    CHECK_NEAR(sink.last_rate, 4.5, 0.01);  // = ShuttleRate(2068)
    r.OnShuttleOffset(-2068);
    CHECK(sink.rate_calls == 2);
    CHECK_NEAR(sink.last_rate, -4.5, 0.01);
    CHECK(sink.last_rate == -ShuttleRate(2068));  // odd symmetry held end-to-end

    // Back to the dead band: one more HardStop (re-armed by the nonzero rate).
    r.OnShuttleOffset(0);
    CHECK(sink.hard_stops == 2);
    CHECK(sink.rate_calls == 2);
  }

  // Scrl OnWheel: the view scrolls by delta × JogEngine::kSecondsPerWheelUnit
  // seconds — the same timeline scale as the jog cursor (same on-screen
  // speed at any zoom). Zero delta does nothing; Jog ignores the delta.
  {
    FakeSink sink;
    WheelState ws;
    Router r(sink, ws);

    r.OnWheel(45);  // Jog: ignored
    CHECK(sink.scroll_calls == 0);

    ws.Set(WheelMode::Scrl);
    r.TakeModeChange(nullptr);

    r.OnWheel(45);
    CHECK(sink.scroll_calls == 1);
    CHECK(sink.scroll_dt_sum == 45.0 * JogEngine::kSecondsPerWheelUnit);

    r.OnWheel(-45);
    CHECK(sink.scroll_calls == 2);  // same magnitude, opposite direction
    CHECK(sink.scroll_dt_sum == 0.0);  // exact round trip

    r.OnWheel(0);
    CHECK(sink.scroll_calls == 2);  // no motion → no call

    // Back to Jog: the view is not touched again.
    ws.Set(WheelMode::Jog);
    r.TakeModeChange(nullptr);
    r.OnWheel(1000);
    CHECK(sink.scroll_calls == 2);
  }

  return check_failures();
}
