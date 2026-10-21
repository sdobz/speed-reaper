// led_read_test.cpp — the LED state reads against the primitives-only
// FakeProject: the M8 LED snapshot (docs/archive/plugin-design-m5-m9.md
// §5, docs/notes/m8-state-reads.md), driven through BindingLedMask with
// the embedded default bindings (reads: action 1157 / 41117 / 40310 /
// 40311 / take N / isolate / solo) plus SharedActiveTake (project_ops.h).
#include "fake_project.h"

#include "bindings.h"
#include "check.h"
#include "led_state.h"
#include "project_ops.h"
#include "speed_editor/key_map.h"
#include "verb_registry.h"
#include "verbs.h"

namespace {

constexpr std::uint32_t Bit(int led) { return 1u << led; }

ItemInfo Item(int id, int track, bool selected) {
  ItemInfo it;
  it.id = id;
  it.track = track;
  it.pos = 0.0;
  it.len = 1.0;
  it.selected = selected;
  return it;
}

}  // namespace

int main() {
  const VerbRegistry reg = MakeDefaultRegistry();
  const Bindings def = ParseBindings(DefaultBindingsText(), reg);
  CHECK(def.errors.empty());

  // Default: nothing lit.
  {
    FakeProject p;
    const std::uint32_t mask =
        BindingLedMask(def, reg, LedReadContext{p, false, false});
    CHECK(mask == 0u);
  }

  // Snap / ripple / trim-behind map straight through the primitives
  // (ToggleState 1157/41117, RippleMode): SNAP, CUT (per-track),
  // DIS (all), LIVE_OWR.
  {
    FakeProject p;
    p.snap = true;
    p.ripple = 2;
    p.trim_behind = true;
    std::uint32_t mask =
        BindingLedMask(def, reg, LedReadContext{p, false, false});
    CHECK(mask == (Bit(LED_SNAP) | Bit(LED_DIS) | Bit(LED_LIVE_OWR)));
    p.ripple = 1;  // per-track: CUT on, DIS off
    mask = BindingLedMask(def, reg, LedReadContext{p, false, false});
    CHECK(mask == (Bit(LED_SNAP) | Bit(LED_CUT) | Bit(LED_LIVE_OWR)));
  }

  // SharedActiveTake: 1-based active take shared by ALL selected items.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_indices = {1, 1};  // both on take index 1 -> take number 2
    CHECK(SharedActiveTake(p) == 2);
    p.take_indices = {1, 2};  // mixed -> 0
    CHECK(SharedActiveTake(p) == 0);
  }

  // Unselected items do not count; nothing selected -> 0.
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, true)};
    p.take_indices = {0, 1};
    CHECK(SharedActiveTake(p) == 2);  // only item 1 (take 2) counts
    p.items = {Item(0, 0, false)};
    p.take_indices = {0};
    CHECK(SharedActiveTake(p) == 0);
  }

  // A plain item (no takes entry) is a 1-take item -> take 1.
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    CHECK(SharedActiveTake(p) == 1);
  }

  // Take numbers above 9 have no CAM bit -> 0, and nothing lights.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_indices = {9, 9};  // take number 10 -> out of range
    CHECK(SharedActiveTake(p) == 0);
    CHECK(BindingLedMask(def, reg, LedReadContext{p, false, false}) == 0u);
  }

  // The shared take drives the CAM bits through the `take N` reads.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_indices = {1, 1};  // shared take number 2
    const std::uint32_t mask =
        BindingLedMask(def, reg, LedReadContext{p, false, false});
    CHECK(mask == Bit(LED_CAM2));
  }

  // Isolate / solo are the view-op state, carried by the read context.
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    IsolateView iso;
    SoloView solo;
    iso.Toggle(p, {0});
    solo.Toggle(p, {0});
    CHECK(iso.IsActive());
    CHECK(solo.IsActive());
    p.items[0].selected = false;  // no selection: the CAM bits stay dark
    const std::uint32_t both =
        BindingLedMask(def, reg, LedReadContext{p, true, true});
    CHECK(both == (Bit(LED_VIDEO_ONLY) | Bit(LED_AUDIO_ONLY)));
    iso.Toggle(p, {0});
    solo.Toggle(p, {0});
    CHECK(!iso.IsActive());
    CHECK(!solo.IsActive());
    CHECK(BindingLedMask(def, reg, LedReadContext{p, false, false}) == 0u);
  }

  return check_failures();
}
