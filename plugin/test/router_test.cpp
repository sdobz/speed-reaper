// Characterizes Router (docs/plugin-design-m1-m3.md §6, design §8 /
// docs/plugin-design-m4-shuttle-impl.md for SHTL): press rules, STOP/PLAY,
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

ButtonChange Down(std::uint16_t key) { return {key, true}; }
ButtonChange Up(std::uint16_t key) { return {key, false}; }

bool Take(WheelMode *m, Router &r) {
  *m = WheelMode::Shtl;  // poison
  return r.TakeModeChange(m);
}

}  // namespace

int main() {
  // Initial mode is Jog; nothing pending, nothing sent to the sink.
  {
    FakeSink sink;
    Router r(sink);
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));
    CHECK(sink.command_count == 0 && sink.scroll_calls == 0);
    CHECK(ProtocolMode(WheelMode::Jog) == 0);
    CHECK(ProtocolMode(WheelMode::Scrl) == 2);
    CHECK(ProtocolMode(WheelMode::Shtl) == 3);
  }

  // Press rules: JOG/SCRL presses change the mode, TakeModeChange fires
  // exactly once per change with the new mode; releases are ignored.
  {
    FakeSink sink;
    Router r(sink);

    WheelMode m;
    r.OnButton(Down(KEY_SCRL));
    CHECK(r.Mode() == WheelMode::Scrl);
    CHECK(Take(&m, r) && m == WheelMode::Scrl);
    CHECK(!r.TakeModeChange(nullptr));  // fires once

    r.OnButton(Down(KEY_JOG));
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(Take(&m, r) && m == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));

    // Re-pressing the active mode is not a new change.
    r.OnButton(Down(KEY_JOG));
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));

    // Releases never change the mode.
    r.OnButton(Up(KEY_JOG));
    r.OnButton(Up(KEY_SCRL));
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));
    CHECK(sink.command_count == 0 && sink.scroll_calls == 0);
  }

  // SHTL press enters Shtl mode (M4); JOG hands back off; outside SHTL the
  // absolute offset is inert.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_SHTL));
    CHECK(r.Mode() == WheelMode::Shtl);
    WheelMode m;
    CHECK(Take(&m, r) && m == WheelMode::Shtl);
    r.OnButton(Up(KEY_SHTL));  // release: ignored
    CHECK(r.Mode() == WheelMode::Shtl);
    CHECK(!r.TakeModeChange(nullptr));

    // Parked wheel (center) while shuttling: dead-band pause, no rate call.
    r.OnShuttleOffset(0);
    CHECK(sink.hard_stops == 1 && sink.rate_calls == 0);

    // JOG press hands off: rate 1.0, back to relative mode.
    r.OnButton(Down(KEY_JOG));
    r.TakeModeChange(nullptr);
    CHECK(sink.rate_calls == 1);
    CHECK_NEAR(sink.last_rate, 1.0, 0.001);
    CHECK(r.Mode() == WheelMode::Jog);

    // Out of SHTL: the absolute offset is a no-op.
    r.OnShuttleOffset(2068);
    CHECK(sink.rate_calls == 1 && sink.hard_stops == 1);
  }

  // SHTL law (design §8): OnShuttleOffset feeds ShuttleRate to the sink.
  // Dead band (offset 0) → one HardStop, no rate call, no rewind; leaving
  // the band and re-entering re-arms the stop. ±2068 → ±4.5.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_SHTL));
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

  // STOP/PLAY while shuttling: hard stop, exit SHTL to the prior mode,
  // no RunCommand, no further rate traffic after the exit.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_SCRL));
    r.TakeModeChange(nullptr);
    r.OnButton(Down(KEY_SHTL));
    r.TakeModeChange(nullptr);

    r.OnShuttleOffset(4096);
    CHECK(sink.rate_calls == 1);
    CHECK_NEAR(sink.last_rate, 8.0, 0.01);

    r.OnButton(Down(KEY_STOP_PLAY));
    CHECK(sink.hard_stops == 1);  // hard stop (OnStopButton in REAPER)
    CHECK(sink.command_count == 0);  // not the 40044 toggle
    CHECK(r.Mode() == WheelMode::Scrl);  // back to the prior mode
    WheelMode m;
    CHECK(Take(&m, r) && m == WheelMode::Scrl);

    // Out of SHTL: the offset is inert again.
    r.OnShuttleOffset(4096);
    CHECK(sink.rate_calls == 1 && sink.hard_stops == 1);

    // Re-enter SHTL (prior mode now Scrl again); STOP/PLAY exits to Scrl.
    r.OnButton(Down(KEY_SHTL));
    r.TakeModeChange(nullptr);
    r.OnButton(Down(KEY_STOP_PLAY));
    CHECK(r.Mode() == WheelMode::Scrl);
    CHECK(sink.hard_stops == 2);
  }

  // STOP/PLAY outside SHTL still uses the 40044 toggle (M2 behavior), and
  // the initial prior mode for a STOP/PLAY SHTL exit is Jog.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_SHTL));  // prior mode: Jog
    r.TakeModeChange(nullptr);
    r.OnButton(Down(KEY_STOP_PLAY));
    CHECK(sink.hard_stops == 1);
    CHECK(r.Mode() == WheelMode::Jog);

    r.OnButton(Down(KEY_STOP_PLAY));  // outside SHTL: the toggle
    CHECK(sink.command_count == 1);
    CHECK(sink.last_command == 40044);
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(sink.hard_stops == 1);
  }

  // JOG/SCRL tap while shuttling hands off: rate 1.0 + relative mode.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_SHTL));
    r.TakeModeChange(nullptr);
    r.OnShuttleOffset(2068);
    CHECK(sink.rate_calls == 1);

    r.OnButton(Down(KEY_JOG));
    CHECK(sink.rate_calls == 2);
    CHECK_NEAR(sink.last_rate, 1.0, 0.001);  // hand-off: rate back to 1.0
    CHECK(r.Mode() == WheelMode::Jog);
    WheelMode m;
    CHECK(Take(&m, r) && m == WheelMode::Jog);

    // JOG press outside SHTL: no rate traffic at all.
    r.OnButton(Down(KEY_SCRL));
    r.TakeModeChange(nullptr);
    r.OnButton(Down(KEY_JOG));
    CHECK(sink.rate_calls == 2);
  }

  // STOP/PLAY down → RunCommand(40044) once; release ignored; mode unchanged.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_STOP_PLAY));
    CHECK(sink.command_count == 1);
    CHECK(sink.last_command == 40044);
    r.OnButton(Up(KEY_STOP_PLAY));
    CHECK(sink.command_count == 1);
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));
  }

  // Scrl OnWheel: the view scrolls by delta × JogEngine::kSecondsPerWheelUnit
  // seconds — the same timeline scale as the jog cursor (same on-screen
  // speed at any zoom). Zero delta does nothing; Jog ignores the delta.
  {
    FakeSink sink;
    Router r(sink);

    r.OnWheel(45);  // Jog: ignored
    CHECK(sink.scroll_calls == 0);

    r.OnButton(Down(KEY_SCRL));
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
    r.OnButton(Down(KEY_JOG));
    r.TakeModeChange(nullptr);
    r.OnWheel(1000);
    CHECK(sink.scroll_calls == 2);
  }

  return check_failures();
}
