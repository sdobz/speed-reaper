// Characterizes Router + ModeGatedSource (docs/plugin-design-m1-m3.md §6):
// press rules, STOP/PLAY, Scrl seconds-based view scrolling (same scale as
// the jog cursor), and mode gating of the engine source. No REAPER, no HID.
#include "router.h"

#include "check.h"
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

  int command_count = 0;
  int last_command = 0;
  int scroll_calls = 0;
  double scroll_dt_sum = 0.0;
};

// Deterministic IWheelSource: returns the configured sample on every Tick.
class FakeWheelSource : public IWheelSource {
 public:
  void Tick(WheelSample *out) override { *out = sample_; }
  WheelSample sample_{};
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

  // SHTL is ignored until M4: mode unchanged, no mode change, no action.
  {
    FakeSink sink;
    Router r(sink);
    r.OnButton(Down(KEY_SHTL));
    r.OnButton(Up(KEY_SHTL));
    CHECK(r.Mode() == WheelMode::Jog);
    CHECK(!r.TakeModeChange(nullptr));
    CHECK(sink.command_count == 0 && sink.scroll_calls == 0);
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

  // ModeGatedSource: Jog passes the delta through; Scrl outputs 0 and
  // routes the delta to the Router (horizontal scroll).
  {
    FakeSink sink;
    Router r(sink);
    FakeWheelSource hid;
    ModeGatedSource gated(hid, r);

    hid.sample_ = {45};
    WheelSample out{};
    gated.Tick(&out);
    CHECK(out.delta == 45);  // Jog: playhead motion, no scroll
    CHECK(sink.scroll_calls == 0);

    r.OnButton(Down(KEY_SCRL));
    r.TakeModeChange(nullptr);
    out = {};
    gated.Tick(&out);
    CHECK(out.delta == 0);  // Scrl: engine sees nothing
    CHECK(sink.scroll_calls == 1);  // 45 units → one scroll right
    CHECK(sink.scroll_dt_sum == 45.0 * JogEngine::kSecondsPerWheelUnit);

    hid.sample_ = {-45};
    out = {};
    gated.Tick(&out);
    CHECK(out.delta == 0);
    CHECK(sink.scroll_calls == 2);
    CHECK(sink.scroll_dt_sum == 0.0);  // round trip

    r.OnButton(Down(KEY_JOG));
    r.TakeModeChange(nullptr);
    hid.sample_ = {7};
    out = {};
    gated.Tick(&out);
    CHECK(out.delta == 7);  // back to Jog: passthrough
    CHECK(sink.scroll_calls == 2);
  }

  return check_failures();
}
