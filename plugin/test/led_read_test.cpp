// led_read_test.cpp — FakeProject::ReadLedState: the M8 LED snapshot
// (docs/plugin-design-m5-m9.md §5, docs/notes/m8-state-reads.md).
#include "fake_project.h"

#include "check.h"

namespace {

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
  // Default: nothing lit.
  {
    FakeProject p;
    LedState s = p.ReadLedState();
    CHECK(!s.snap);
    CHECK(s.ripple == 0);
    CHECK(!s.trim_behind);
    CHECK(s.active_take == 0);
    CHECK(!s.isolate);
    CHECK(!s.solo);
    CHECK(!p.IsolateActive());
    CHECK(!p.SoloActive());
  }

  // Snap / ripple / trim-behind map straight through.
  {
    FakeProject p;
    p.snap = true;
    p.ripple = 2;
    p.trim_behind = true;
    LedState s = p.ReadLedState();
    CHECK(s.snap);
    CHECK(s.ripple == 2);
    CHECK(s.trim_behind);
  }

  // active_take: 1-based active take shared by ALL selected items.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.active_takes = {1, 1};  // both on take index 1 -> take number 2
    CHECK(p.ReadLedState().active_take == 2);
    p.active_takes = {1, 2};  // mixed -> 0
    CHECK(p.ReadLedState().active_take == 0);
  }

  // Unselected items do not count; nothing selected -> 0.
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, true)};
    p.active_takes = {0, 1};
    CHECK(p.ReadLedState().active_take == 2);  // only item 1 (take 2) counts
    p.items = {Item(0, 0, false)};
    p.active_takes = {0};
    CHECK(p.ReadLedState().active_take == 0);
  }

  // A plain item (no takes entry) is a 1-take item -> take 1.
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    CHECK(p.ReadLedState().active_take == 1);
  }

  // Take numbers above 9 have no CAM bit -> 0 (also: 0-take item, no
  // active take -> 0).
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_counts = {10, 10};
    p.active_takes = {9, 9};  // take number 10 -> out of range
    CHECK(p.ReadLedState().active_take == 0);
    p.take_counts = {0, 1};
    p.active_takes = {0, 0};  // item 0 has no active take
    CHECK(p.ReadLedState().active_take == 0);
  }

  // Isolate / solo come from the M6 toggle state (on -> off again).
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    p.ToggleIsolateTracks({0});
    p.ToggleSoloTracks({0});
    LedState s = p.ReadLedState();
    CHECK(s.isolate);
    CHECK(s.solo);
    CHECK(p.IsolateActive());
    CHECK(p.SoloActive());
    p.ToggleIsolateTracks({0});
    p.ToggleSoloTracks({0});
    s = p.ReadLedState();
    CHECK(!s.isolate);
    CHECK(!s.solo);
    CHECK(!p.IsolateActive());
    CHECK(!p.SoloActive());
  }

  return check_failures();
}
