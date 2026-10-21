// Characterizes Session (40-cleanup §5): reconnect re-sends mode + LEDs;
// the LED mask is pushed only when it changes; a stall > 250 ms is caught
// up at most the 250 ms cap (≤ 50 ms per Tick); a key press reaches the
// verb sink. No REAPER, no HID — fake device + fake_project.h.
#include "session.h"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

#include "bindings.h"  // ParseBindings (expected masks)
#include "check.h"
#include "fake_project.h"
#include "led_state.h"  // BindingLedMask (expected masks)
#include "speed_editor/key_map.h"  // KEY_SNAP
#include "verb_registry.h"  // MakeDefaultRegistry, LedReadContext
#include "verbs.h"  // DefaultBindingsText
#include "wheel_mode.h"  // ProtocolMode

namespace {

class FakeDevice final : public IDevice {
 public:
  void Tick(WheelSample *out) override { *out = sample_; }
  bool PopButton(ButtonChange *out) override {
    if (buttons_.empty()) return false;
    *out = buttons_.front();
    buttons_.pop_front();
    return true;
  }
  void PushButton(ButtonChange b) { buttons_.push_back(b); }
  bool PopLog(std::string *out) override {
    (void)out;
    return false;
  }
  bool Connected() const override { return connected_; }
  std::int32_t AbsolutePosition() const override { return abs_; }
  void RequestWheelMode(std::uint8_t protocolMode) override {
    ++mode_requests_;
    last_mode_ = protocolMode;
  }
  void RequestJogLeds(bool jog, bool shtl, bool scrl) override {
    jog_leds_.push_back({jog, shtl, scrl});
  }
  void RequestLeds(std::uint32_t mainMask) override {
    leds_.push_back(mainMask);
  }

  WheelSample sample_{};
  std::deque<ButtonChange> buttons_;
  bool connected_ = false;
  std::int32_t abs_ = 0;
  int mode_requests_ = 0;
  std::uint8_t last_mode_ = 0;
  std::vector<std::array<bool, 3>> jog_leds_;
  std::vector<std::uint32_t> leds_;
};

class FakeSink final : public IActionSink {
 public:
  void RunCommand(int cmd) override { commands_.push_back(cmd); }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override {}
  void Message(const char *msg) override { messages_.push_back(msg); }
  std::vector<int> commands_;
  std::vector<std::string> messages_;
};

class FakePlayhead final : public IPlayheadSink {
 public:
  void ScrubBySeconds(double dt) override { scrubs_.push_back(dt); }
  double PlayPosition() const override { return 0.0; }
  std::vector<double> scrubs_;
};

class FakeZoom final : public IZoomSource {
 public:
  double px_per_sec = 100.0;
  double PixelsPerSecond() const override { return px_per_sec; }
};

}  // namespace

int main() {
  // Reconnect re-sends mode + LEDs (design §6): the false→true edge
  // re-forwards the current mode and forces the main-panel mask.
  {
    FakeDevice dev;
    FakeProject proj;
    FakeSink sink;
    FakePlayhead ph;
    FakeZoom zoom;
    Session s(dev, sink, proj, ph, zoom);
    s.Tick(0);  // not connected: nothing sent
    CHECK(dev.mode_requests_ == 0);
    CHECK(dev.leds_.empty());

    dev.connected_ = true;  // connect edge on the next tick
    s.Tick(8);
    CHECK(dev.mode_requests_ == 1);
    CHECK(dev.last_mode_ == ProtocolMode(WheelMode::Jog));
    CHECK(dev.jog_leds_.size() == 1);
    CHECK(dev.jog_leds_[0] == (std::array<bool, 3>{true, false, false}));
    CHECK(dev.leds_.size() == 1);
    // Mirror the session: embedded default bindings against the empty
    // project (no view toggles).
    const VerbRegistry reg = MakeDefaultRegistry();
    const Bindings def = ParseBindings(DefaultBindingsText(), reg);
    CHECK(dev.leds_[0] ==
          BindingLedMask(def, reg, LedReadContext{proj, false, false}));

    dev.connected_ = false;
    s.Tick(16);
    CHECK(dev.mode_requests_ == 1);  // disconnect sends nothing
    CHECK(dev.leds_.size() == 1);

    dev.connected_ = true;  // reconnect edge: mode + LEDs re-sent
    s.Tick(24);
    CHECK(dev.mode_requests_ == 2);
    CHECK(dev.last_mode_ == ProtocolMode(WheelMode::Jog));
    CHECK(dev.jog_leds_.size() == 2);
    CHECK(dev.leds_.size() == 2);  // mask forced even though unchanged
    CHECK(dev.leds_[1] == dev.leds_[0]);
  }

  // LED mask pushed only on change (M8): polling every ~250 ms recomputes
  // the mask but pushes to the device only when it differs.
  {
    FakeDevice dev;
    FakeProject proj;
    FakeSink sink;
    FakePlayhead ph;
    FakeZoom zoom;
    Session s(dev, sink, proj, ph, zoom);
    dev.connected_ = true;
    s.Tick(0);
    const std::uint32_t off = dev.leds_.back();
    CHECK(dev.leds_.size() == 1);

    s.Tick(8);    // within the 250 ms window: no poll, no push
    s.Tick(256);  // poll fires, mask unchanged: no push
    CHECK(dev.leds_.size() == 1);

    // Mirror the session: the default bindings' snap read (action 1157)
    // sees proj.snap.
    const VerbRegistry reg = MakeDefaultRegistry();
    const Bindings def = ParseBindings(DefaultBindingsText(), reg);
    proj.snap = true;
    const std::uint32_t snap_mask =
        BindingLedMask(def, reg, LedReadContext{proj, false, false});
    CHECK(snap_mask != off);
    s.Tick(512);  // poll fires, mask changed: pushed exactly once
    CHECK(dev.leds_.size() == 2);
    CHECK(dev.leds_[1] == snap_mask);
    s.Tick(776);  // next poll: unchanged again
    CHECK(dev.leds_.size() == 2);
  }

  // Stall catch-up: a >250 ms gap consumes at most the 250 ms cap, spread
  // over Ticks at the per-Tick cap (7 × 8 ms steps = 56 ms, the first
  // multiple of the 8 ms step above 50 ms). With a constant wheel delta of
  // 1, every engine step produces exactly one scrub call, so the scrub
  // count IS the engine-step count.
  {
    FakeDevice dev;
    FakeProject proj;
    FakeSink sink;
    FakePlayhead ph;
    FakeZoom zoom;
    Session s(dev, sink, proj, ph, zoom);
    dev.connected_ = true;
    dev.sample_ = WheelSample{1};
    s.Tick(0);  // first tick: no gap, no engine step
    CHECK(ph.scrubs_.size() == 0);
    s.Tick(8);  // normal 8 ms cycle: one step
    CHECK(ph.scrubs_.size() == 1);
    s.Tick(1000);  // 992 ms stall: backlog capped to 250 ms, ≤50 ms now
    CHECK(ph.scrubs_.size() <= 1 + 7);  // 7 × 8 ms = 56 ms (step-quantized 50 ms cap)
    CHECK(ph.scrubs_.size() == 8);
    // Drain the capped backlog at 8 ms intervals; it must run out well
    // before the uncapped 1000 ms of wall time would be exhausted.
    for (std::int64_t t = 1008; t <= 1048; t += 8) {
      s.Tick(t);
    }
    CHECK(ph.scrubs_.size() == 38);  // 1 (t=8) + 31 (250 ms backlog) + 6 (t=1008..1040)
    s.Tick(1056);  // steady state: exactly one step per 8 ms
    CHECK(ph.scrubs_.size() == 39);
  }

  // Key press reaches the verb sink: SNAP (not a double-tap key) taps at
  // release and runs its command (1157, "Options: Toggle snapping").
  {
    FakeDevice dev;
    FakeProject proj;
    FakeSink sink;
    FakePlayhead ph;
    FakeZoom zoom;
    Session s(dev, sink, proj, ph, zoom);
    dev.PushButton({KEY_SNAP, true});
    s.Tick(0);
    CHECK(sink.commands_.empty());  // press only: nothing yet
    dev.PushButton({KEY_SNAP, false});
    s.Tick(50);  // release: Tap → VerbRouter → RunCommand
    CHECK(sink.commands_.size() == 1);
    CHECK(sink.commands_[0] == 1157);
  }

  // Learn: the next tap is reported as ("KEY.gesture", "action <spec>") and
  // swallowed (no verb runs); double-tap works; a hold+turn is refused;
  // ESC cancels; 10 s without a gesture times out.
  {
    FakeDevice dev;
    FakeProject proj;
    FakeSink sink;
    FakePlayhead ph;
    FakeZoom zoom;
    Session s(dev, sink, proj, ph, zoom);
    std::vector<std::string> got;
    s.SetLearnCallback([&got](const std::string &l, const std::string &v) {
      got.push_back(l + " = " + v);
    });
    s.Tick(0);
    s.BeginLearn("_RSabc");
    CHECK(s.Learning());
    dev.PushButton({KEY_SNAP, true});
    s.Tick(100);
    dev.PushButton({KEY_SNAP, false});
    s.Tick(150);  // tap: learned, NOT run
    CHECK(!s.Learning());
    CHECK(got.size() == 1 && got[0] == "SNAP.tap = action _RSabc");
    CHECK(sink.commands_.empty());
    // Back to normal: SNAP runs 1157 again.
    dev.PushButton({KEY_SNAP, true});
    s.Tick(200);
    dev.PushButton({KEY_SNAP, false});
    s.Tick(250);
    CHECK(sink.commands_.size() == 1 && sink.commands_[0] == 1157);

    // Double-tap on IN (a double-tap key by default).
    s.BeginLearn("40012");
    for (int i = 0; i < 2; ++i) {
      dev.PushButton({KEY_IN, true});
      s.Tick(1000 + i * 100);
      dev.PushButton({KEY_IN, false});
      s.Tick(1050 + i * 100);
    }
    CHECK(got.size() == 2 && got[1] == "IN.double = action 40012");

    // ESC cancels (ESC is not learnable): nothing reported.
    s.BeginLearn("40012");
    dev.PushButton({KEY_ESC, true});
    s.Tick(2000);
    dev.PushButton({KEY_ESC, false});
    s.Tick(2050);
    s.Tick(2400);  // ESC tap is deferred (it has a .double binding)
    CHECK(!s.Learning());
    CHECK(got.size() == 2);

    // Timeout: 10 s.
    s.BeginLearn("40012");
    s.Tick(5000);
    CHECK(s.Learning());
    s.Tick(12500);
    CHECK(!s.Learning());
    CHECK(got.size() == 2);
  }

  // Reload: new text rebinds; bad lines are reported and skipped.
  {
    FakeDevice dev;
    FakeProject proj;
    FakeSink sink;
    FakePlayhead ph;
    FakeZoom zoom;
    Session s(dev, sink, proj, ph, zoom);
    s.ReloadBindings("[bindings]\nSNAP.tap = action 999\nFOO.tap = action 1\n");
    dev.PushButton({KEY_SNAP, true});
    s.Tick(0);
    dev.PushButton({KEY_SNAP, false});
    s.Tick(50);
    CHECK(sink.commands_.size() == 1 && sink.commands_[0] == 999);
    CHECK(sink.messages_.size() >= 2);
  }

  return check_failures();
}
