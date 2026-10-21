// Implicit-target rule (docs/archive/plugin-design-m5-m9.md §1.1) and
// WithTargetSelected: selected items win; cursor containment on the target
// track; boundary belongs to the item starting there; cursor at the end of
// the track's last item; target-track fallback to last-touched; nothing
// under the cursor → empty; selection snapshot is always restored (including
// "none selected"). No REAPER.
#include "target.h"

#include "check.h"
#include "fake_project.h"

namespace {

ItemInfo Item(int id, int track, double pos, double len, bool selected = false) {
  return {id, track, pos, len, selected};
}

int TargetFirst(IProject &p) {
  const std::vector<int> t = TargetItems(p);
  return t.empty() ? -1 : t.front();
}

void TestSelectedWin() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 1, 0.5, 1.0, true),
             Item(2, 1, 2.0, 1.0, true)};
  p.cursor = 0.5;  // inside item 0 — irrelevant, a selection exists
  p.last_touched = 0;
  CHECK(TargetItems(p) == (std::vector<int>{1, 2}));
}

void TestCursorInsideItem() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 1.5, 1.0)};
  p.cursor = 0.75;
  p.last_touched = 0;
  CHECK(TargetFirst(p) == 0);
  p.cursor = 2.4;
  CHECK(TargetFirst(p) == 1);
}

void TestCursorAtEndOfLastItem() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 1.0, 2.0)};
  p.cursor = 3.0;  // exactly pos+len of item 1, the track's last item
  p.last_touched = 0;
  CHECK(TargetFirst(p) == 1);
  p.cursor = 3.5;  // past the last item → nothing
  CHECK(TargetFirst(p) == -1);
}

void TestBoundaryBelongsToItemStartingThere() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 1.0, 1.0)};
  p.cursor = 1.0;  // end of item 0 == start of item 1
  p.last_touched = 0;
  CHECK(TargetFirst(p) == 1);
}

void TestTargetTrackFallbacks() {
  // No selected track → last-touched track is searched.
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 1, 2.0, 1.0)};
  p.cursor = 0.5;
  p.last_touched = 1;  // item 0 is under the cursor but on track 0
  CHECK(TargetFirst(p) == -1);  // nothing on track 1 under the cursor
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 1, 0.25, 1.0)};
  CHECK(TargetFirst(p) == 1);

  // A selected track beats last-touched: first selected track wins.
  FakeProject q;
  q.items = {Item(0, 0, 0.25, 1.0), Item(1, 2, 0.25, 1.0)};
  q.tracks = {2, 0};
  q.last_touched = 0;
  q.cursor = 0.5;
  CHECK(TargetFirst(q) == 1);  // track 2 is first in SelectedTracks()

  // No track selected and no last-touched track → empty.
  FakeProject r;
  r.items = {Item(0, 0, 0.0, 1.0)};
  r.cursor = 0.5;
  r.last_touched = -1;
  CHECK(TargetItems(r).empty());
}

void TestNothingUnderCursor() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 2.0, 1.0)};
  p.cursor = 1.5;  // in the gap
  p.last_touched = 0;
  CHECK(TargetItems(p).empty());
}

void TestCursorItemOnTrack() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 1.0, 1.0),  // track 0
             Item(7, 1, 0.25, 1.0)};  // other track
  const auto on = [&](int track, double cursor) {
    return CursorItemOnTrack(p.items, track, cursor);
  };
  CHECK(on(0, 0.5) == 0);   // inside item 0
  CHECK(on(0, 1.5) == 1);   // inside item 1
  CHECK(on(0, 1.0) == 1);   // boundary → the item starting there
  CHECK(on(0, 2.0) == 1);   // exactly at the last item's end → that item
  CHECK(on(0, -0.1) == -1); // before the first item
  CHECK(on(0, 2.5) == -1);  // past the last item
  // Item 7 on track 1 overlaps the cursor at 0.75: it is the answer on its
  // own track and ignored for track 0 (where item 0 answers instead).
  CHECK(on(1, 0.75) == 7);
  CHECK(on(0, 0.75) == 0);
}

void TestWithTargetSelectedRestores() {
  // No selection + target: select targets for f, then restore "none".
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 1.0, 1.0)};
  p.cursor = 1.5;
  p.last_touched = 0;
  int f_calls = 0;
  WithTargetSelected(p, [&] { ++f_calls; });
  CHECK(f_calls == 1);
  CHECK(p.selected_calls.size() == 2);
  CHECK(p.selected_calls.front() == (std::vector<int>{1}));
  CHECK(p.selected_calls.back().empty());  // restored "none selected"

  // With a selection: f runs under the existing selection, no extra select.
  FakeProject q;
  q.items = {Item(0, 0, 0.0, 1.0), Item(1, 0, 1.0, 1.0, true)};
  q.cursor = 0.5;
  q.last_touched = 0;
  WithTargetSelected(q, [] {});
  CHECK(q.selected_calls.size() == 1);
  CHECK(q.selected_calls.front() == (std::vector<int>{1}));  // restored as-is

  // No selection and no target: nothing is selected, snapshot (none)
  // restored.
  FakeProject r;
  r.items = {Item(0, 0, 0.0, 1.0)};
  r.cursor = 9.0;
  r.last_touched = 0;
  WithTargetSelected(r, [] {});
  CHECK(r.selected_calls.size() == 1);
  CHECK(r.selected_calls.front().empty());
}

}  // namespace

int main() {
  TestSelectedWin();
  TestCursorInsideItem();
  TestCursorAtEndOfLastItem();
  TestBoundaryBelongsToItemStartingThere();
  TestTargetTrackFallbacks();
  TestNothingUnderCursor();
  TestCursorItemOnTrack();
  TestWithTargetSelectedRestores();
  return check_failures();
}
