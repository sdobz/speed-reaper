// led_state_test.cpp — MainLedMask(const LedState&): design §5 bit
// mapping; BindingLedMask(const Bindings&, ...): design § Mask.
#include "led_state.h"

#include "bindings.h"
#include "check.h"
#include "fake_project.h"
#include "speed_editor/key_map.h"
#include "verb_registry.h"

namespace {

constexpr std::uint32_t Bit(int led) { return 1u << led; }

// One selected item on track 0 whose shared active take (1-based) is
// `take1` (FakeProject active_takes is 0-based take indices).
void OneSelectedTake(FakeProject &p, int take1) {
  p.items.push_back(ItemInfo{0, 0, 0.0, 1.0, true});
  p.tracks = {0};
  p.take_counts = {take1};
  p.active_takes = {take1 - 1};
}

}  // namespace

int main() {
  // Default state: nothing lit.
  CHECK(MainLedMask(LedState{}) == 0u);

  // Snap only.
  {
    LedState s;
    s.snap = true;
    CHECK(MainLedMask(s) == Bit(LED_SNAP));
  }

  // Ripple: per-track -> CUT only; all -> DIS only; off -> none.
  {
    LedState s;
    s.ripple = 1;
    CHECK(MainLedMask(s) == Bit(LED_CUT));
    s.ripple = 2;
    CHECK(MainLedMask(s) == Bit(LED_DIS));
    s.ripple = 0;
    CHECK(MainLedMask(s) == 0u);
  }

  // Trim-behind -> LIVE_OWR.
  {
    LedState s;
    s.trim_behind = true;
    CHECK(MainLedMask(s) == Bit(LED_LIVE_OWR));
  }

  // Active take: 2 -> CAM2 only; 0 -> none; out of range -> none.
  {
    LedState s;
    s.active_take = 2;
    CHECK(MainLedMask(s) == Bit(LED_CAM2));
    s.active_take = 0;
    CHECK(MainLedMask(s) == 0u);
    s.active_take = 12;
    CHECK(MainLedMask(s) == 0u);
  }

  // Isolate / solo bits.
  {
    LedState s;
    s.isolate = true;
    CHECK(MainLedMask(s) == Bit(LED_VIDEO_ONLY));
    s.isolate = false;
    s.solo = true;
    CHECK(MainLedMask(s) == Bit(LED_AUDIO_ONLY));
  }

  // Combinations OR together (and stay within the known LED bits).
  {
    LedState s;
    s.snap = true;
    s.ripple = 1;
    s.trim_behind = true;
    s.active_take = 7;
    s.isolate = true;
    s.solo = true;
    const std::uint32_t expected =
        Bit(LED_SNAP) | Bit(LED_CUT) | Bit(LED_LIVE_OWR) | Bit(LED_CAM7) |
        Bit(LED_VIDEO_ONLY) | Bit(LED_AUDIO_ONLY);
    CHECK(MainLedMask(s) == expected);
  }

  // ── BindingLedMask (design § Mask) ─────────────────────────────────
  FakeProject proj;
  const VerbRegistry reg = MakeDefaultRegistry();
  LedReadContext ctx{proj, false, false};

  // Gesture priority: tap → double → turn → press. The parser forbids
  // mixing `press` with other gestures on one key (press-exclusivity),
  // so the bindings are constructed directly. Constant reads make each
  // winner unambiguous: tap=true, double=false, turn=true, press=false.
  {
    struct ConstVerb final : public Verb {
      void Run(VerbContext &) override {}
    };
    const auto factory = [](std::vector<std::string>) {
      return VerbBuildResult{std::make_unique<ConstVerb>(), ""};
    };
    const auto const_read = [](bool v) {
      return [v](const std::vector<std::string> &) -> std::optional<StateRead> {
        return StateRead{[v](const LedReadContext &) { return v; }};
      };
    };
    VerbRegistry pr;
    pr.Register(VerbSpec{"vtap", {BindingGesture::Tap}, "tap read",
                         factory, const_read(true)});
    pr.Register(VerbSpec{"vdouble", {BindingGesture::Double}, "double read",
                         factory, const_read(false)});
    pr.Register(VerbSpec{"vturn", {BindingGesture::Turn}, "turn read",
                         factory, const_read(true)});
    pr.Register(VerbSpec{"vpress", {BindingGesture::Press}, "press read",
                         factory, const_read(false)});

    // Tap bound (with all the others): lit ⇒ tap's true read beat
    // double's false read.
    {
      Bindings b;
      b.bindings[{KEY_CUT, BindingGesture::Tap}] =
          BoundVerb{"vtap", {}, nullptr};
      b.bindings[{KEY_CUT, BindingGesture::Double}] =
          BoundVerb{"vdouble", {}, nullptr};
      b.bindings[{KEY_CUT, BindingGesture::Turn}] =
          BoundVerb{"vturn", {}, nullptr};
      b.bindings[{KEY_CUT, BindingGesture::Press}] =
          BoundVerb{"vpress", {}, nullptr};
      CHECK(BindingLedMask(b, pr, ctx) == Bit(LED_CUT));
    }
    // No tap: double bound before turn; dark ⇒ double's false read beat
    // turn's true read (and the press was never consulted).
    {
      Bindings b;
      b.bindings[{KEY_CUT, BindingGesture::Double}] =
          BoundVerb{"vdouble", {}, nullptr};
      b.bindings[{KEY_CUT, BindingGesture::Turn}] =
          BoundVerb{"vturn", {}, nullptr};
      b.bindings[{KEY_CUT, BindingGesture::Press}] =
          BoundVerb{"vpress", {}, nullptr};
      CHECK(BindingLedMask(b, pr, ctx) == 0u);
    }
    // No tap/double: turn bound before press; lit ⇒ turn's true read beat
    // press's false read.
    {
      Bindings b;
      b.bindings[{KEY_CUT, BindingGesture::Turn}] =
          BoundVerb{"vturn", {}, nullptr};
      b.bindings[{KEY_CUT, BindingGesture::Press}] =
          BoundVerb{"vpress", {}, nullptr};
      CHECK(BindingLedMask(b, pr, ctx) == Bit(LED_CUT));
    }
  }

  // A double-only key (no tap) uses its double binding.
  {
    const Bindings b = ParseBindings(
        "[bindings]\nCAM3.double = action 1157\n", reg);
    CHECK(b.errors.empty());
    proj.snap = true;
    CHECK(BindingLedMask(b, reg, ctx) == Bit(LED_CAM3));
    proj.snap = false;
    CHECK(BindingLedMask(b, reg, ctx) == 0u);
  }

  // `action 1157` (snap) lights the SNAP bit when snap is on.
  {
    const Bindings b = ParseBindings(
        "[bindings]\nSNAP.tap = action 1157\n", reg);
    CHECK(b.errors.empty());
    proj.snap = true;
    CHECK(BindingLedMask(b, reg, ctx) == Bit(LED_SNAP));
    proj.snap = false;
    CHECK(BindingLedMask(b, reg, ctx) == 0u);
  }

  // `action 41193` (not in ActionStateReads) keeps its key dark even
  // with other state on.
  {
    const Bindings b = ParseBindings(
        "[bindings]\nSMTH_CUT.tap = action 41193\n", reg);
    CHECK(b.errors.empty());
    proj.snap = true;
    proj.ripple = 1;
    CHECK(BindingLedMask(b, reg, ctx) == 0u);
    proj.snap = false;
    proj.ripple = 0;
  }

  // `take N` lights the matching CAM bit for the shared active take.
  {
    const Bindings b = ParseBindings(
        "[bindings]\nCAM4.tap = take 4\n", reg);
    CHECK(b.errors.empty());
    OneSelectedTake(proj, 4);
    CHECK(BindingLedMask(b, reg, ctx) == Bit(LED_CAM4));
    OneSelectedTake(proj, 5);
    CHECK(BindingLedMask(b, reg, ctx) == 0u);
    proj.items.clear();
    proj.tracks.clear();
    proj.take_counts.clear();
    proj.active_takes.clear();
  }

  // Isolate / solo bits follow the read context.
  {
    const Bindings b = ParseBindings(
        "[bindings]\n"
        "VIDEO_ONLY.tap = isolate_tracks\n"
        "AUDIO_ONLY.tap = solo_tracks\n",
        reg);
    CHECK(b.errors.empty());
    LedReadContext both{proj, true, true};
    CHECK(BindingLedMask(b, reg, both) ==
          (Bit(LED_VIDEO_ONLY) | Bit(LED_AUDIO_ONLY)));
    LedReadContext iso{proj, true, false};
    CHECK(BindingLedMask(b, reg, iso) == Bit(LED_VIDEO_ONLY));
    LedReadContext solo{proj, false, true};
    CHECK(BindingLedMask(b, reg, solo) == Bit(LED_AUDIO_ONLY));
    CHECK(BindingLedMask(b, reg, ctx) == 0u);
  }

  // Unbound keys stay dark: an empty file lights nothing even with a
  // fully lit project state.
  {
    const Bindings b = ParseBindings("[bindings]\n", reg);
    CHECK(b.errors.empty());
    FakeProject lit;
    lit.snap = true;
    lit.ripple = 2;
    OneSelectedTake(lit, 2);
    LedReadContext c{lit, true, true};
    CHECK(BindingLedMask(b, reg, c) == 0u);
  }
  return check_failures();
}
