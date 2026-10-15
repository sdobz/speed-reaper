// led_read_test.cpp — the ReadLedState op (project_ops.h) against the
// primitives-only FakeProject: the M8 LED snapshot
// (docs/archive/plugin-design-m5-m9.md §5, docs/notes/m8-state-reads.md).
#include "fake_project.h"
#include "project_ops.h"

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
    LedState s = ReadLedState(p, false, false);
    CHECK(!s.snap);
    CHECK(s.ripple == 0);
    CHECK(!s.trim_behind);
    CHECK(s.active_take == 0);
    CHECK(!s.isolate);
    CHECK(!s.solo);
  }

  // Snap / ripple / trim-behind map straight through the primitives
  // (ToggleState 1157/41117, RippleMode).
  {
    FakeProject p;
    p.snap = true;
    p.ripple = 2;
    p.trim_behind = true;
    LedState s = ReadLedState(p, false, false);
    CHECK(s.snap);
    CHECK(s.ripple == 2);
    CHECK(s.trim_behind);
  }

  // active_take: 1-based active take shared by ALL selected items.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.active_takes = {1, 1};  // both on take index 1 -> take number 2
    CHECK(ReadLedState(p, false, false).active_take == 2);
    p.active_takes = {1, 2};  // mixed -> 0
    CHECK(ReadLedState(p, false, false).active_take == 0);
  }

  // Unselected items do not count; nothing selected -> 0.
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, true)};
    p.active_takes = {0, 1};
    CHECK(ReadLedState(p, false, false).active_take == 2);  // only item 1 (take 2) counts
    p.items = {Item(0, 0, false)};
    p.active_takes = {0};
    CHECK(ReadLedState(p, false, false).active_take == 0);
  }

  // A plain item (no takes entry) is a 1-take item -> take 1.
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    CHECK(ReadLedState(p, false, false).active_take == 1);
  }

  // Take numbers above 9 have no CAM bit -> 0.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.active_takes = {9, 9};  // take number 10 -> out of range
    CHECK(ReadLedState(p, false, false).active_take == 0);
  }

  // Isolate / solo are the view-op state, passed straight through.
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    IsolateView iso;
    SoloView solo;
    iso.Toggle(p, {0});
    solo.Toggle(p, {0});
    CHECK(iso.IsActive());
    CHECK(solo.IsActive());
    LedState s = ReadLedState(p, iso.IsActive(), solo.IsActive());
    CHECK(s.isolate);
    CHECK(s.solo);
    iso.Toggle(p, {0});
    solo.Toggle(p, {0});
    s = ReadLedState(p, iso.IsActive(), solo.IsActive());
    CHECK(!iso.IsActive());
    CHECK(!solo.IsActive());
    CHECK(!s.isolate);
    CHECK(!s.solo);
  }

  return check_failures();
}
