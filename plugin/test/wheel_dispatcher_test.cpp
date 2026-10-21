// Characterizes WheelDispatcher (40-cleanup §6): one pure class decides
// where each tick's wheel input goes — active hold → GestureEngine::OnWheel,
// else by mode: Jog → JogEngine (passthrough), Scrl → Router::OnWheel,
// Shtl → Router::OnShuttleOffset(absolute). One case per branch, plus
// "hold wins over every mode" (the routing assertions ported from
// router_test.cpp / session_test.cpp). No REAPER, no HID.
#include "wheel_dispatcher.h"

#include "check.h"
#include "shuttle.h"  // ShuttleRate: the law the Shtl branch must forward
#include "speed_editor/key_map.h"  // KEY_JOG, KEY_SCRL, KEY_SHTL, KEY_SNAP

namespace {

// Deterministic IWheelDevice: configured sample on every Tick.
class FakeDevice final : public IWheelDevice {
 public:
  void Tick(WheelSample *out) override { *out = sample_; }
  std::int32_t AbsolutePosition() const override { return abs_; }
  WheelSample sample_{};
  std::int32_t abs_ = 0;
};

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


void DrainWheel(GestureEngine &g) {
  Gesture gest;
  while (g.Pop(&gest)) {}
}

}  // namespace

int main() {
  // Jog branch: the delta passes through to the engine untouched; no
  // scroll, no shuttle traffic (even with a nonzero absolute position).
  {
    FakeDevice dev;
    FakeSink sink;
    GestureEngine gestures;
    WheelState ws;
    Router r(sink, ws);
    WheelDispatcher d(dev, gestures, r);

    dev.abs_ = 2068;  // parked off-center: must not trigger a rate call
    dev.sample_ = WheelSample{45};
    WheelSample out{};
    d.Tick(&out);
    CHECK(out.delta == 45);  // Jog: playhead motion
    CHECK(sink.scroll_calls == 0);
    CHECK(sink.rate_calls == 0);
    CHECK(sink.hard_stops == 0);

    dev.sample_ = WheelSample{0};
    out = {};
    d.Tick(&out);
    CHECK(out.delta == 0);  // no motion → no motion
  }

  // Scrl branch: the view scrolls by delta × kSecondsPerWheelUnit seconds
  // (same scale as the jog cursor); the engine sees 0. Back in Jog the
  // delta passes through and the view is not touched again.
  {
    FakeDevice dev;
    FakeSink sink;
    GestureEngine gestures;
    WheelState ws;
    Router r(sink, ws);
    WheelDispatcher d(dev, gestures, r);

    ws.Set(WheelMode::Scrl);
    r.TakeModeChange(nullptr);

    dev.sample_ = WheelSample{45};
    WheelSample out{};
    d.Tick(&out);
    CHECK(out.delta == 0);  // Scrl: engine sees nothing
    CHECK(sink.scroll_calls == 1);  // 45 units → one scroll right
    CHECK(sink.scroll_dt_sum == 45.0 * JogEngine::kSecondsPerWheelUnit);

    dev.sample_ = WheelSample{-45};
    out = {};
    d.Tick(&out);
    CHECK(out.delta == 0);
    CHECK(sink.scroll_calls == 2);  // same magnitude, opposite direction
    CHECK(sink.scroll_dt_sum == 0.0);  // exact round trip

    ws.Set(WheelMode::Jog);
    r.TakeModeChange(nullptr);
    dev.sample_ = WheelSample{7};
    out = {};
    d.Tick(&out);
    CHECK(out.delta == 7);  // back to Jog: passthrough
    CHECK(sink.scroll_calls == 2);
  }

  // Shtl branch: the wheel DELTA is ignored; the absolute position feeds
  // the SHTL law (ShuttleRate), re-applied every tick. Dead band → one
  // HardStop.
  {
    FakeDevice dev;
    FakeSink sink;
    GestureEngine gestures;
    WheelState ws;
    Router r(sink, ws);
    WheelDispatcher d(dev, gestures, r);

    ws.Set(WheelMode::Shtl);
    r.TakeModeChange(nullptr);

    dev.abs_ = 2068;
    dev.sample_ = WheelSample{12};  // delta must be inert in Shtl
    WheelSample out{};
    d.Tick(&out);
    CHECK(out.delta == 0);  // no playhead motion
    CHECK(sink.scroll_calls == 0);
    CHECK(sink.rate_calls == 1);
    CHECK_NEAR(sink.last_rate, ShuttleRate(2068), 0.01);  // ≈ 4.5
    d.Tick(&out);  // absolute: re-applied every tick, no integration
    CHECK(sink.rate_calls == 2);

    dev.abs_ = 0;  // parked (center): dead-band pause, no rate call
    d.Tick(&out);
    CHECK(sink.hard_stops == 1);
    CHECK(sink.rate_calls == 2);
    d.Tick(&out);
    CHECK(sink.hard_stops == 1);  // parked: no re-issue
  }

  // A held turn key wins over every mode (design §7): one key press held
  // across Jog, Scrl and Shtl — the wheel always drives the held key's
  // HoldRotate; none of the three destinations sees it. (KEY_TIMELINE is a
  // tap+turn key in the defaults; it is registered here as the turn set.)
  {
    FakeDevice dev;
    FakeSink sink;
    GestureEngine gestures;
    gestures.SetTurnKeys({KEY_TIMELINE});
    WheelState ws;
    Router r(sink, ws);
    WheelDispatcher d(dev, gestures, r);

    dev.abs_ = 2068;  // off-center from the start
    gestures.OnKey(KEY_TIMELINE, true, 0);  // press (Jog mode, initial)
    gestures.Poll(350);                     // hold fires: HoldStart
    {
      Gesture gest;
      CHECK(gestures.Pop(&gest) && gest.kind == GestureKind::HoldStart &&
            gest.key == KEY_TIMELINE);
      DrainWheel(gestures);
    }

    // Jog + hold: passthrough suppressed, no scroll/shuttle traffic.
    dev.sample_ = WheelSample{45};
    WheelSample out{};
    d.Tick(&out);
    CHECK(out.delta == 0);
    CHECK(sink.scroll_calls == 0 && sink.rate_calls == 0);
    CHECK(sink.hard_stops == 0);
    Gesture gest;
    CHECK(gestures.Pop(&gest) && gest.kind == GestureKind::HoldRotate &&
          gest.key == KEY_TIMELINE && gest.delta == 45);

    // Scrl + hold: the view is not scrolled either.
    ws.Set(WheelMode::Scrl);
    r.TakeModeChange(nullptr);
    dev.sample_ = WheelSample{9};
    out = {};
    d.Tick(&out);
    CHECK(out.delta == 0);
    CHECK(sink.scroll_calls == 0);
    CHECK(gestures.Pop(&gest) && gest.kind == GestureKind::HoldRotate &&
          gest.delta == 9);

    // Shtl + hold: no shuttle rate traffic either.
    ws.Set(WheelMode::Shtl);
    r.TakeModeChange(nullptr);
    dev.sample_ = WheelSample{5};
    out = {};
    d.Tick(&out);
    CHECK(out.delta == 0);
    CHECK(sink.rate_calls == 0 && sink.hard_stops == 0);
    CHECK(gestures.Pop(&gest) && gest.kind == GestureKind::HoldRotate &&
          gest.delta == 5);
    DrainWheel(gestures);
  }

  // 45-cam-n: a tap-only key held while coasting does NOT own the wheel. In
  // Jog the delta passes through to the engine (the cursor keeps moving) and
  // no HoldStart/HoldRotate is emitted for the tap key — so its release stays
  // a Tap and the selection is not swallowed by a phantom hold.
  {
    FakeDevice dev;
    FakeSink sink;
    GestureEngine gestures;
    gestures.SetTurnKeys({KEY_TIMELINE});  // only TIMELINE is a turn key
    WheelState ws;
    Router r(sink, ws);
    WheelDispatcher d(dev, gestures, r);

    dev.abs_ = 2068;
    gestures.OnKey(KEY_CAM3, true, 0);  // hold a tap-only key (CAM3)
    dev.sample_ = WheelSample{45};
    WheelSample out{};
    d.Tick(&out);
    CHECK(out.delta == 45);  // Jog: still jogs while CAM3 is held
    CHECK(sink.scroll_calls == 0 && sink.rate_calls == 0);
    CHECK(sink.hard_stops == 0);
    {
      Gesture gest;
      CHECK(!gestures.Pop(&gest));  // no HoldStart/HoldRotate for a tap key
    }
    // A turn key held instead still captures the wheel (contrast).
    gestures.OnKey(KEY_TIMELINE, true, 1000);
    gestures.Poll(1350);  // hold fires: HoldStart
    {
      Gesture g0;
      CHECK(gestures.Pop(&g0) && g0.kind == GestureKind::HoldStart &&
            g0.key == KEY_TIMELINE);
      DrainWheel(gestures);
    }
    dev.sample_ = WheelSample{45};
    out = {};
    d.Tick(&out);
    CHECK(out.delta == 0);  // turn key owns the wheel
    Gesture gest;
    CHECK(gestures.Pop(&gest) && gest.kind == GestureKind::HoldRotate);
  }

  return check_failures();
}
