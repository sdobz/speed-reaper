// led_state_test.cpp — BindingLedMask(const Bindings&, ...): design
// § Mask.
#include "led_state.h"

#include "bindings.h"
#include "check.h"
#include "default_bindings_generated.h"
#include "fake_project.h"
#include "speed_editor/key_map.h"
#include "verb_registry.h"

namespace {

constexpr std::uint32_t Bit(int led) { return 1u << led; }

// One selected item on track 0 whose shared active take (1-based) is
// `take1` (FakeProject take_indices is 0-based take indices).
void OneSelectedTake(FakeProject &p, int take1) {
  p.items.push_back(ItemInfo{0, 0, 0.0, 1.0, true});
  p.tracks = {0};
  p.take_counts = {take1};
  p.take_indices = {take1 - 1};
}

}  // namespace

int main() {
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
    proj.take_indices.clear();
  }

  // The real defaults (kDefaultBindingsIni): select_track LEDs follow the
  // selection — no key press. Two tracks, one item each spanning the
  // cursor; selecting the track-2 item lights only CAM2. A CAM key bound
  // to another track follows that track; a non-CAM key bound to
  // select_track lights its own bit.
  {
    FakeProject p;
    p.items.push_back(ItemInfo{0, 0, 0.0, 2.0, false});  // track 1, under cursor
    p.items.push_back(ItemInfo{1, 1, 0.0, 2.0, true});   // track 2, under cursor
    p.cursor = 1.0;
    LedReadContext c{p, false, false};
    {
      const Bindings b = ParseBindings(kDefaultBindingsIni, reg);
      CHECK(b.errors.empty());
      CHECK(BindingLedMask(b, reg, c) == Bit(LED_CAM2));
      p.items[1].selected = false;
      p.items[0].selected = true;  // selection on track 1 → CAM1 follows
      CHECK(BindingLedMask(b, reg, c) == Bit(LED_CAM1));
      p.items[0].selected = false;  // nothing selected → no CAM bit
      CHECK(BindingLedMask(b, reg, c) == 0u);
      p.items[1].selected = true;
    }
    // CAM1 bound to select_track 3 follows track 3 (its own track's item,
    // though selected under the cursor, is not followed).
    p.items.push_back(ItemInfo{2, 2, 0.0, 2.0, true});  // track 3, under cursor
    {
      const Bindings b = ParseBindings(
          "[bindings]\nCAM1.tap = select_track 3\n", reg);
      CHECK(b.errors.empty());
      CHECK(BindingLedMask(b, reg, c) == Bit(LED_CAM1));
      p.items[2].selected = false;
      CHECK(BindingLedMask(b, reg, c) == 0u);
      p.items[2].selected = true;
    }
    // A non-CAM key bound to select_track 2 lights its own bit.
    {
      const Bindings b = ParseBindings(
          "[bindings]\nCLOSE_UP.tap = select_track 2\n", reg);
      CHECK(b.errors.empty());
      CHECK(BindingLedMask(b, reg, c) == Bit(LED_CLOSE_UP));
      p.items[1].selected = false;
      CHECK(BindingLedMask(b, reg, c) == 0u);
    }
    p.items.clear();
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
