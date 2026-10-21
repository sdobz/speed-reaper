// project_ops_test.cpp — the pure project ops (project_ops.h, 40-cleanup
// §8) against the primitives-only FakeProject: DuplicateItemsAbove and
// the IsolateView / SoloView toggles with their saved re-tap state.
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
  // DuplicateItemsAbove: one item on track 0 → the copy sits on the NEW
  // track 0 (above the source track, which shifts down to track 1), same
  // position, unselected; the original and its selection are untouched.
  {
    FakeProject p;
    p.items = {Item(0, 0, true)};
    DuplicateItemsAbove(p, {0});
    CHECK((p.insert_calls == std::vector<int>{0}));  // parens: macro arg
    CHECK(p.copy_calls.size() == 1);
    CHECK(p.copy_calls[0].first == 0);
    CHECK(p.copy_calls[0].second == 0);
    CHECK(p.items.size() == 2);
    CHECK(p.items[0].id == 0);
    CHECK(p.items[0].track == 1);      // original shifted down
    CHECK(p.items[0].pos == 0.0);
    CHECK(p.items[0].selected == true);  // selection untouched
    CHECK(p.items[1].track == 0);       // the copy, on the new track
    CHECK(p.items[1].pos == 0.0);       // same position
    CHECK(p.items[1].selected == false);
  }

  // DuplicateItemsAbove: two source tracks. The op walks the tracks in
  // descending order (an insert at t shifts only tracks above t); every
  // item lands on a new track above its source.
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, false)};  // A: t0, B: t1
    DuplicateItemsAbove(p, {0, 1});
    CHECK((p.insert_calls == std::vector<int>{1, 0}));  // descending
    CHECK(p.copy_calls.size() == 2);
    CHECK(p.copy_calls[0].first == 1);  // B first (higher track)
    CHECK(p.copy_calls[0].second == 1);
    CHECK(p.copy_calls[1].first == 0);
    CHECK(p.copy_calls[1].second == 0);
    CHECK(p.items.size() == 4);
    // Final track layout: [copyA(0), A(1), copyB(2), B(3)] — each copy sits
    // on the new track inserted above its source (an insert at t shifts
    // every track at/above t, so A's own insert moves A from 0 to 1).
    CHECK(p.items[0].track == 1);  // A (original)
    CHECK(p.items[1].track == 3);  // B (original)
    CHECK(p.items[2].track == 2);  // B's copy
    CHECK(p.items[3].track == 0);  // A's copy
    CHECK(p.items[2].selected == false);
    CHECK(p.items[3].selected == false);
  }

  // DuplicateItemsAbove: bad ids are ignored (no primitive calls at all
  // when no valid id names a track).
  {
    FakeProject p;
    p.items = {Item(0, 0, false)};
    DuplicateItemsAbove(p, {99});
    CHECK(p.insert_calls.empty());
    CHECK(p.copy_calls.empty());
    CHECK(p.items.size() == 1);
  }

  // IsolateView: every track hidden except the target items' tracks; the
  // second tap restores the saved B_SHOWINTCP/B_SHOWINMIXER EXACTLY
  // (track 1 was mixer-hidden beforehand → restored hidden).
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, false)};
    p.track_views = {{true, true, 0}, {true, false, 0}};
    IsolateView v;
    CHECK(!v.IsActive());

    v.Toggle(p, {0});
    CHECK(v.IsActive());
    CHECK(p.track_views[0].show_tcp == true);
    CHECK(p.track_views[0].show_mixer == true);
    CHECK(p.track_views[1].show_tcp == false);  // hidden
    CHECK(p.track_views[1].show_mixer == false);

    v.Toggle(p, {0});
    CHECK(!v.IsActive());
    CHECK(p.track_views[0].show_tcp == true);
    CHECK(p.track_views[0].show_mixer == true);
    CHECK(p.track_views[1].show_tcp == true);     // restored
    CHECK(p.track_views[1].show_mixer == false);  // restored exactly
  }

  // IsolateView: with no per-track state recorded yet, TrackCount derives
  // from the items' tracks; the second tap restores the (default) flags.
  {
    FakeProject p;
    p.items = {Item(0, 0, false)};
    IsolateView v;
    v.Toggle(p, {0});
    CHECK(p.track_views.size() == 1);  // grown by SetTrackFlag
    CHECK(p.track_views[0].show_tcp == true);  // target kept visible
    v.Toggle(p, {0});
    CHECK(!v.IsActive());
    CHECK(p.track_views[0].show_tcp == true);
  }

  // SoloView: I_SOLO on the target tracks only; the second tap restores
  // exactly (track 0 was already solo, stays solo; untouched by the op).
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, false)};
    p.track_views = {{true, true, 1}, {true, true, 0}};
    SoloView v;
    CHECK(!v.IsActive());

    v.Toggle(p, {1});
    CHECK(v.IsActive());
    CHECK(p.track_views[0].solo == 1);  // untouched
    CHECK(p.track_views[1].solo == 1);  // soloed

    v.Toggle(p, {1});
    CHECK(!v.IsActive());
    CHECK(p.track_views[0].solo == 1);  // untouched
    CHECK(p.track_views[1].solo == 0);  // restored exactly
  }

  // A toggle with no valid target ids: a no-op that does not toggle the
  // saved state (the op is re-armable later).
  {
    FakeProject p;
    p.items = {Item(0, 0, false)};
    IsolateView iso;
    SoloView solo;
    iso.Toggle(p, {99});
    CHECK(!iso.IsActive());
    CHECK(p.track_views.empty());
    solo.Toggle(p, {});
    CHECK(!solo.IsActive());
    iso.Toggle(p, {0});  // still armable
    CHECK(iso.IsActive());
    CHECK(p.track_views[0].show_tcp == true);
  }

  // SharedActiveTake: the 1-based active take shared by ALL selected
  // items (I_CURTAKE only).
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_indices = {1, 1};  // both on take index 1 -> take number 2
    CHECK(SharedActiveTake(p) == 2);
    // A plain 1-take item has active take index 0 -> take number 1.
    FakeProject one;
    one.items = {Item(0, 0, true)};
    CHECK(SharedActiveTake(one) == 1);
  }

  // SharedActiveTake: 0 when nothing is selected (items exist but are
  // unselected, and when there are no items at all).
  {
    FakeProject p;
    p.items = {Item(0, 0, false), Item(1, 1, false)};
    p.take_indices = {1, 1};
    CHECK(SharedActiveTake(p) == 0);
    FakeProject none;
    CHECK(SharedActiveTake(none) == 0);
  }

  // SharedActiveTake: 0 when the selected items disagree (mixed takes),
  // and 0 when a selected item has no active take.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_indices = {1, 2};  // mixed -> 0
    CHECK(SharedActiveTake(p) == 0);
    p.take_indices = {1, -1};  // item 1 has no active take -> 0
    CHECK(SharedActiveTake(p) == 0);
  }

  // SharedActiveTake: 0 when the shared take is > 9 (no CAM LED for it);
  // take number 9 (index 8) is the highest one that counts.
  {
    FakeProject p;
    p.items = {Item(0, 0, true), Item(1, 1, true)};
    p.take_indices = {8, 8};  // take number 9 -> 9
    CHECK(SharedActiveTake(p) == 9);
    p.take_indices = {9, 9};  // take number 10 -> 0
    CHECK(SharedActiveTake(p) == 0);
    p.take_indices = {8, 9};  // mixed, one beyond 9 -> 0
    CHECK(SharedActiveTake(p) == 0);
  }

  return check_failures();
}
