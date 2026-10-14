// verbs_test.cpp — VerbRouter tables (design §7; M5 design §2 composites).
// Time is not involved: gestures come in pre-recognized, via a fake
// IActionSink and a FakeProject.
#include <algorithm>  // std::find (ApplyLastSelection)
#include <string>     // std::string (FakeSink::Message recording)

#include "verbs.h"

#include "check.h"
#include "fake_project.h"
#include "speed_editor/key_map.h"

namespace {

// Records every command the router runs.
class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { commands.push_back(cmd); }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override {}
  void Message(const char *msg) override { messages.push_back(msg); }

  std::vector<int> commands;
  std::vector<std::string> messages;
};

Gesture G(std::uint16_t key, GestureKind kind, std::int32_t delta = 0) {
  return Gesture{key, kind, delta};
}

// The FakeProject records SetSelectedItems but does not apply them; in REAPER
// the selection IS applied, so between gestures the test pushes the last
// recorded selection into items[].selected.
void ApplyLastSelection(FakeProject &project) {
  if (project.selected_calls.empty()) return;
  const auto &sel = project.selected_calls.back();
  for (auto &item : project.items)
    item.selected =
        std::find(sel.begin(), sel.end(), item.id) != sel.end();
}

void test_mapped_gestures() {
  FakeSink sink;
  FakeProject project;
  VerbRouter r(sink, project);

  r.OnGesture(G(KEY_SPLIT, GestureKind::Tap));  // SPLIT Tap → 40012
  CHECK(sink.commands.size() == 1);
  CHECK(sink.commands[0] == 40012);

  r.OnGesture(G(KEY_ESC, GestureKind::Tap));  // ESC Tap → 40635 (remove time selection)
  CHECK(sink.commands.size() == 2);
  CHECK(sink.commands[1] == 40635);

  r.OnGesture(G(KEY_ESC, GestureKind::DoubleTap));  // ESC DoubleTap → 40029 (Undo)
  CHECK(sink.commands.size() == 3);
  CHECK(sink.commands[2] == 40029);

  r.OnGesture(G(KEY_IN, GestureKind::Tap));  // IN Tap → 40625 (set start point)
  CHECK(sink.commands.size() == 4);
  CHECK(sink.commands[3] == 40625);

  r.OnGesture(G(KEY_IN, GestureKind::DoubleTap));  // IN DoubleTap → 40635
  CHECK(sink.commands.size() == 5);
  CHECK(sink.commands[4] == 40635);

  r.OnGesture(G(KEY_AUDIO_LEVEL, GestureKind::DoubleTap));  // M6: → 40157 (insert marker)
  CHECK(sink.commands.size() == 6);
  CHECK(sink.commands[5] == 40157);
  CHECK(sink.messages.empty());  // read-out is the Tap composite, not the DoubleTap
}

void test_unmapped_gestures_do_nothing() {
  FakeSink sink;
  FakeProject project;
  VerbRouter r(sink, project);

  r.OnGesture(G(KEY_ROLL, GestureKind::Tap));  // not in the table
  CHECK(sink.commands.empty());

  r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 3));  // hold verbs: no-op in M3
  CHECK(sink.commands.empty());

  r.OnGesture(G(KEY_IN, GestureKind::HoldStart));
  r.OnGesture(G(KEY_ESC, GestureKind::HoldEnd));
  CHECK(sink.commands.empty());
}

// M5 design §3: TRIM IN / OUT run their ids inside WithTargetSelected and
// one undo block labelled "Speed Reaper: trim" (composite rows win over id
// rows; the id rows for 40511/40512 no longer exist).
void test_trim_composites() {
  FakeSink sink;
  FakeProject project;
  project.items.push_back(ItemInfo{0, 0, 1.0, 2.0, false});
  project.cursor = 1.5;  // inside item 0; nothing selected
  project.last_touched = 0;
  VerbRouter r(sink, project);

  r.OnGesture(G(KEY_TRIM_IN, GestureKind::Tap));
  CHECK(sink.commands.size() == 1);
  CHECK(sink.commands[0] == 40511);
  CHECK(project.undo_begins == 1);
  CHECK(project.undo_ends == 1);
  CHECK(project.last_undo_label == "Speed Reaper: trim");
  // WithTargetSelected: select the implicit target, then restore the empty
  // original selection.
  CHECK(project.selected_calls.size() == 2);
  CHECK(project.selected_calls[0].size() == 1);
  CHECK(project.selected_calls[0][0] == 0);
  CHECK(project.selected_calls[1].empty());

  r.OnGesture(G(KEY_TRIM_OUT, GestureKind::Tap));
  CHECK(sink.commands.size() == 2);
  CHECK(sink.commands[1] == 40512);
  CHECK(project.undo_begins == 2);
  CHECK(project.undo_ends == 2);
  CHECK(project.last_undo_label == "Speed Reaper: trim");

  // Rule 1: an existing selection wins; the verb runs with it and restores
  // it unchanged.
  FakeSink sink2;
  FakeProject project2;
  project2.items.push_back(ItemInfo{0, 0, 1.0, 2.0, true});
  project2.last_touched = 0;
  VerbRouter r2(sink2, project2);
  r2.OnGesture(G(KEY_TRIM_IN, GestureKind::Tap));
  CHECK(sink2.commands.size() == 1);
  CHECK(sink2.commands[0] == 40511);
  CHECK(project2.selected_calls.size() == 1);  // snapshot restore only
  CHECK(project2.selected_calls[0].size() == 1);
  CHECK(project2.selected_calls[0][0] == 0);
}

// M5 design §3: RIPL DEL — time selection present → exactly [40201]; else
// T (one undo block): ripple off → [40310, 40006, 40309], ripple on →
// [40006]; no targets → trace `target none`, do nothing. The selection is
// restored after every call.
void test_ripl_del_composite() {
  // 1) Time selection present: exactly [40201]; no target/selection churn,
  // no undo block.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 1.0, 2.0, false});
    project.time_sel = true;
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_RIPL_DEL, GestureKind::Tap));
    CHECK(sink.commands.size() == 1);
    CHECK(sink.commands[0] == 40201);
    CHECK(project.selected_calls.empty());
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
  }

  // 2) Ripple off + one target: [40310, 40006, 40309] inside one undo block;
  // target selected, then the original (empty) selection restored.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;  // inside item 0; nothing selected
    project.last_touched = 0;
    project.ripple = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_RIPL_DEL, GestureKind::Tap));
    CHECK(sink.commands.size() == 3);
    CHECK(sink.commands[0] == 40310);
    CHECK(sink.commands[1] == 40006);
    CHECK(sink.commands[2] == 40309);
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.selected_calls.size() == 2);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);
    CHECK(project.selected_calls[1].empty());  // selection restored
  }

  // 3) Ripple per-track: just [40006]; selection restored the same way.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;
    project.last_touched = 0;
    project.ripple = 1;  // per-track
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_RIPL_DEL, GestureKind::Tap));
    CHECK(sink.commands.size() == 1);
    CHECK(sink.commands[0] == 40006);
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.selected_calls.size() == 2);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);
    CHECK(project.selected_calls[1].empty());
  }

  // 4) No targets and no time selection: no commands, no selection churn,
  // no undo block (the trace line is greppable via SPEED_REAPER_TRACE).
  {
    FakeSink sink;
    FakeProject project;  // no items at all
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_RIPL_DEL, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(project.selected_calls.empty());
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
  }

  // 5) An existing selection wins and is restored unchanged (single
  // snapshot-restore call, no temporary select of the implicit target).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, true});
    project.cursor = 2.5;
    project.last_touched = 0;
    project.ripple = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_RIPL_DEL, GestureKind::Tap));
    CHECK(sink.commands.size() == 3);
    CHECK(sink.commands[0] == 40310);
    CHECK(sink.commands[1] == 40006);
    CHECK(sink.commands[2] == 40309);
    CHECK(project.selected_calls.size() == 1);  // snapshot restore only
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);
  }
}

void test_double_tap_keys() {
  auto keys = VerbDoubleTapKeys();
  CHECK(keys.size() == 5);
  CHECK(keys[0] == KEY_IN);
  CHECK(keys[1] == KEY_OUT);
  CHECK(keys[2] == KEY_ESC);
  CHECK(keys[3] == KEY_TIMELINE);
  CHECK(keys[4] == KEY_AUDIO_LEVEL);
}

// M5 design §1.2: TIMELINE explicit-select composites. The FakeProject does
// not apply SetSelectedItems, so "the state after a tap" is simulated by
// setting ItemInfo.selected directly.
void test_timeline_gestures() {
  // Two adjacent items on the target track, cursor inside A, nothing
  // selected (last-touched track is the target track).
  auto make_project = []() {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A
    project.items.push_back(ItemInfo{1, 0, 5.0, 5.0, false});  // B
    project.cursor = 2.5;
    project.last_touched = 0;
    return project;
  };

  // 1) Tap: select the rule-2 target (cursor item) exclusively.
  {
    FakeSink sink;
    FakeProject project = make_project();
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);  // A
  }

  // 2) Tap again with A already the sole selection: deselect all.
  {
    FakeSink sink;
    FakeProject project = make_project();
    project.items[0].selected = true;
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::Tap));
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].empty());
  }

  // 3) Tap with B selected (A still the cursor target): A selected
  // exclusively.
  {
    FakeSink sink;
    FakeProject project = make_project();
    project.items[1].selected = true;
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::Tap));
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);
  }

  // 4) Tap with the cursor in a gap: no target, no selection change.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.items.push_back(ItemInfo{1, 0, 6.0, 4.0, false});
    project.cursor = 4.5;  // between the items
    project.last_touched = 0;
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(project.selected_calls.empty());
  }

  // 5) DoubleTap: exactly 40289, no selection churn.
  {
    FakeSink sink;
    FakeProject project = make_project();
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::DoubleTap));
    CHECK(sink.commands.size() == 1);
    CHECK(sink.commands[0] == 40289);
    CHECK(project.selected_calls.empty());
  }

  // 6) HoldRotate: one step per kSelectStepTicks (=20) accumulated ticks,
  // clamped at the ends, cursor untouched. 19+1 ticks = one step. Each
  // step's selection is applied before the next gesture (REAPER applies
  // SetSelectedItems; the fake only records it).
  {
    FakeSink sink;
    FakeProject project = make_project();
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, 19));
    CHECK(project.selected_calls.empty());  // below the step threshold

    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, 1));  // total 20
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);  // from nothing: first item
    ApplyLastSelection(project);

    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, 20));
    CHECK(project.selected_calls.size() == 2);
    CHECK(project.selected_calls[1][0] == 1);  // next: B
    ApplyLastSelection(project);

    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, 20));
    CHECK(project.selected_calls.size() == 3);
    CHECK(project.selected_calls[2][0] == 1);  // clamped at the end
    ApplyLastSelection(project);

    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, -20));
    CHECK(project.selected_calls.size() == 4);
    CHECK(project.selected_calls[3][0] == 0);  // previous: A
    ApplyLastSelection(project);

    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, -40));  // two steps
    CHECK(project.selected_calls.size() == 6);
    CHECK(project.selected_calls[4][0] == 0);  // clamped at the start
    CHECK(project.selected_calls[5][0] == 0);
    ApplyLastSelection(project);

    CHECK(sink.commands.empty());               // pure selection, no commands
    CHECK(project.cursor == 2.5);               // cursor untouched
  }

  // 7) HoldRotate from nothing selected, negative first step: last item.
  {
    FakeSink sink;
    FakeProject project = make_project();
    VerbRouter r(sink, project);
    r.OnGesture(G(KEY_TIMELINE, GestureKind::HoldRotate, -20));
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 1);
  }
}

// M6 design §3: CAM 1–9 set the active take N (I_CURTAKE = N-1) on each
// target item; items with fewer than N takes keep theirs. One undo block
// labelled "Speed Reaper: take N"; the selection is restored (rule 1).
void test_cam_takes() {
  // 1) CAM2 with two targets (the existing selection, rule 1): the 3-take
  // item is set to index 1; the 1-take item is untouched.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, true});  // A, selected, 3 takes
    project.items.push_back(ItemInfo{1, 0, 5.0, 5.0, true});  // B, selected, 1 take
    project.take_counts = {3, 1};
    project.active_takes = {0, 0};
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_CAM2, GestureKind::Tap));
    CHECK(sink.commands.empty());  // pure project API, no REAPER commands
    CHECK(project.set_take_calls.size() == 1);
    CHECK(project.set_take_calls[0].first == 0);  // item A
    CHECK(project.set_take_calls[0].second == 1);  // take 2 → index 1
    CHECK(project.active_takes[0] == 1);
    CHECK(project.active_takes[1] == 0);  // fewer than 2 takes: untouched
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: take 2");
    // Rule 1: existing selection — snapshot restore only, no temp select.
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].size() == 2);
  }

  // 2) CAM1 with no target (cursor in a gap, nothing selected, no
  // last-touched track): nothing happens — no take set, no undo block, no
  // selection churn (the trace line is `target none`).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 10.0;  // after the item
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_CAM1, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(project.set_take_calls.empty());
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
    CHECK(project.selected_calls.empty());
  }
}

// M6 design §3: AUDIO LEVEL Tap is a pure read-out of the first target
// item's volume (`Volume: <x.x> dB`, 20*log10(D_VOL)) via IActionSink::
// Message; no commands, no selection churn, no undo block. SMTH CUT Tap is
// 40916 with a time selection and nothing (trace `no time selection`
// otherwise).
void test_audio_level_and_smth_cut() {
  // 1) AUDIO LEVEL Tap: D_VOL 0.5 → "Volume: -6.0 dB" on the first target
  // item (cursor target, rule 2); the second item's volume is ignored.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A: D_VOL 0.5
    project.items.push_back(ItemInfo{1, 0, 5.0, 5.0, false});  // B: D_VOL 2.0
    project.item_volumes = {0.5, 2.0};
    project.cursor = 2.5;  // inside A
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_AUDIO_LEVEL, GestureKind::Tap));
    CHECK(sink.commands.empty());  // pure read-out, no REAPER commands
    CHECK(sink.messages.size() == 1);
    CHECK(sink.messages[0] == "Volume: -6.0 dB");
    CHECK(project.selected_calls.empty());
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
  }

  // 2) AUDIO LEVEL Tap with an existing selection: the first SELECTED item
  // (rule 1) is read out — D_VOL 1.0 → "Volume: 0.0 dB".
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.items.push_back(ItemInfo{1, 0, 5.0, 5.0, true});  // B selected
    project.item_volumes = {0.5, 1.0};
    project.cursor = 2.5;
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_AUDIO_LEVEL, GestureKind::Tap));
    CHECK(sink.messages.size() == 1);
    CHECK(sink.messages[0] == "Volume: 0.0 dB");
    CHECK(project.selected_calls.empty());  // read-out never touches the selection
  }

  // 3) AUDIO LEVEL Tap with no target: no message, no commands (trace
  // `target none`).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.cursor = 5.0;  // in a gap
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_AUDIO_LEVEL, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(sink.messages.empty());
    CHECK(project.selected_calls.empty());
  }

  // 4) SMTH CUT Tap with a time selection: exactly [40916].
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.time_sel = true;
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_SMTH_CUT, GestureKind::Tap));
    CHECK(sink.commands.size() == 1);
    CHECK(sink.commands[0] == 40916);
    CHECK(project.selected_calls.empty());
    CHECK(project.undo_begins == 0);
  }

  // 5) SMTH CUT Tap without a time selection: no commands (trace `no time
  // selection`).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.time_sel = false;
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_SMTH_CUT, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(project.selected_calls.empty());
  }
}

// M6 design §3: PLACE ON TOP Tap duplicates the target items onto one new
// track above each source track (IProject::DuplicateItemsAbove with the
// target ids), one undo block "Speed Reaper: place on top"; the original
// selection is restored. No targets → nothing.
void test_place_on_top() {
  // 1) One target (the cursor target, rule 2): the project gets exactly one
  // call with [0]; the fake applies the duplication (copy on the new track
  // 1, same position, unselected; original on track 0 untouched). The
  // verb selects the target, then restores the original (empty) selection.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A on track 0
    project.cursor = 2.5;  // inside A; nothing selected
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_PLACE_ON_TOP, GestureKind::Tap));
    CHECK(sink.commands.empty());  // pure project API, no REAPER commands
    CHECK(project.duplicate_calls.size() == 1);
    CHECK(project.duplicate_calls[0].size() == 1);
    CHECK(project.duplicate_calls[0][0] == 0);
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: place on top");
    CHECK(project.selected_calls.size() == 2);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 0);
    CHECK(project.selected_calls[1].empty());  // original selection restored
    // The fake applied the duplication: A (the original) shifts down to
    // track 1, unselected; the copy sits on the NEW track 0 — above the
    // source track — at the same position, unselected.
    CHECK(project.items.size() == 2);
    CHECK(project.items[0].id == 0);
    CHECK(project.items[0].track == 1);
    CHECK(project.items[0].pos == 0.0);
    CHECK(project.items[0].selected == false);
    CHECK(project.items[1].track == 0);
    CHECK(project.items[1].pos == 0.0);
    CHECK(project.items[1].selected == false);
  }

  // 2) No target (cursor in a gap, nothing selected, no last-touched
  // track): no duplication, no undo block, no selection churn (the trace
  // line is `target none`).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.cursor = 5.0;  // in a gap
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_PLACE_ON_TOP, GestureKind::Tap));
    CHECK(sink.commands.empty());
    CHECK(project.duplicate_calls.empty());
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
    CHECK(project.selected_calls.empty());
  }
}

// M6 design §3: the view buttons. VIDEO_ONLY toggles the isolate view
// (every track hidden except the target items' tracks; the second tap
// restores the saved B_SHOWINTCP/B_SHOWINMIXER exactly). AUDIO_ONLY
// toggles I_SOLO on the target items' tracks (same save/restore). FULL
// VIEW runs 40346 (candidate, settled by the M6 task-6 oracle). The saved
// state is plugin memory only — no undo block, no selection churn.
void test_view_buttons() {
  // 1) Isolate, direct on the fake with the target ids: A item on track 0,
  // B item on track 1; track 1 was mixer-hidden beforehand. The first call
  // hides track 1 (keeps track 0 shown); the second restores track 1's
  // saved flags EXACTLY (tcp back to shown, mixer still hidden).
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A: track 0
    project.items.push_back(ItemInfo{1, 1, 0.0, 5.0, false});  // B: track 1
    project.track_views = {{true, true, 0}, {true, false, 0}};

    project.ToggleIsolateTracks({0});
    CHECK(project.isolate_calls.size() == 1);
    CHECK(project.isolate_calls[0].size() == 1);
    CHECK(project.isolate_calls[0][0] == 0);
    CHECK(project.isolate_active);
    CHECK(project.track_views[0].show_tcp == true);
    CHECK(project.track_views[0].show_mixer == true);
    CHECK(project.track_views[1].show_tcp == false);  // hidden
    CHECK(project.track_views[1].show_mixer == false);

    project.ToggleIsolateTracks({0});
    CHECK(project.isolate_calls.size() == 2);
    CHECK(!project.isolate_active);
    CHECK(project.track_views[0].show_tcp == true);
    CHECK(project.track_views[0].show_mixer == true);
    CHECK(project.track_views[1].show_tcp == true);     // restored
    CHECK(project.track_views[1].show_mixer == false);  // restored exactly
  }

  // 2) Solo, direct on the fake: track 0 was already solo. The toggle
  // affects only the target track; the second tap restores exactly.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A: track 0
    project.items.push_back(ItemInfo{1, 1, 0.0, 5.0, false});  // B: track 1
    project.track_views = {{true, true, 1}, {true, true, 0}};

    project.ToggleSoloTracks({1});
    CHECK(project.solo_calls.size() == 1);
    CHECK(project.solo_calls[0][0] == 1);
    CHECK(project.solo_active);
    CHECK(project.track_views[0].solo == 1);  // untouched
    CHECK(project.track_views[1].solo == 1);  // soloed

    project.ToggleSoloTracks({1});
    CHECK(project.solo_calls.size() == 2);
    CHECK(!project.solo_active);
    CHECK(project.track_views[0].solo == 1);  // untouched
    CHECK(project.track_views[1].solo == 0);  // restored exactly
  }

  // 3) A toggle with no valid target ids: recorded but no state change.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.ToggleIsolateTracks({99});
    CHECK(project.isolate_calls.size() == 1);
    CHECK(!project.isolate_active);
    CHECK(project.track_views.empty());
    project.ToggleSoloTracks({});
    CHECK(project.solo_calls.size() == 1);
    CHECK(!project.solo_active);
  }

  // 4) Routing: VIDEO_ONLY / AUDIO_ONLY taps pass the target ids (cursor
  // target, rule 2) to the project toggles; FULL VIEW runs 40346. No
  // commands from the toggles, no selection churn, no undo block.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A: track 0
    project.items.push_back(ItemInfo{1, 1, 0.0, 5.0, false});  // B: track 1
    project.cursor = 2.5;  // inside A
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_VIDEO_ONLY, GestureKind::Tap));
    CHECK(project.isolate_calls.size() == 1);
    CHECK(project.isolate_calls[0].size() == 1);
    CHECK(project.isolate_calls[0][0] == 0);
    CHECK(project.isolate_active);
    CHECK(project.track_views[1].show_tcp == false);  // track 1 hidden

    r.OnGesture(G(KEY_AUDIO_ONLY, GestureKind::Tap));
    CHECK(project.solo_calls.size() == 1);
    CHECK(project.solo_calls[0].size() == 1);
    CHECK(project.solo_calls[0][0] == 0);
    CHECK(project.solo_active);
    CHECK(project.track_views[0].solo == 1);

    r.OnGesture(G(KEY_FULL_VIEW, GestureKind::Tap));
    CHECK(sink.commands.size() == 1);
    CHECK(sink.commands[0] == 40346);
    CHECK(project.selected_calls.empty());  // no selection churn
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
  }

  // 5) No target (cursor in a gap, nothing selected, no last-touched
  // track): neither toggle is called, no commands (trace `target none`).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.cursor = 5.0;  // in a gap
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_VIDEO_ONLY, GestureKind::Tap));
    r.OnGesture(G(KEY_AUDIO_ONLY, GestureKind::Tap));
    CHECK(project.isolate_calls.empty());
    CHECK(project.solo_calls.empty());
    CHECK(sink.commands.empty());
    CHECK(project.selected_calls.empty());
  }
}

// M7 design §4: hold+rotate. One hold = one undo block; targets resolved
// once at HoldStart; 4 wheel ticks per step of 1/Fps(); selection
// snapshotted at HoldStart and restored at HoldEnd.
void test_hold_rotate() {
  // 1) SPLIT (MOVE) hold, cursor target, 30 fps: +8 ticks → pos +2 frames
  // (2/30 s); exactly one Undo_Begin/Undo_End pair; the release restores
  // the (empty) selection snapshot.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;  // inside item 0; nothing selected
    project.last_touched = 0;
    project.fps = 30.0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldStart));
    CHECK(project.undo_begins == 1);  // the hold's single undo block
    CHECK(project.undo_ends == 0);

    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 3));
    CHECK(project.items[0].pos == 0.0);  // 3 ticks < one step (4)

    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 5));  // 8 total → 2 steps
    CHECK_NEAR(project.items[0].pos, 2.0 / 30.0, 1e-12);  // +2 frames
    CHECK(project.undo_begins == 1);  // still one block for the whole hold
    CHECK(project.undo_ends == 0);
    CHECK(sink.commands.empty());  // pure project API, no REAPER commands

    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldEnd));
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: move");
    CHECK(project.selected_calls.size() == 1);  // snapshot restore on release
    CHECK(project.selected_calls[0].empty());

    // After the hold: a stray HoldRotate/HoldEnd is a no-op.
    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 8));
    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldEnd));
    CHECK_NEAR(project.items[0].pos, 2.0 / 30.0, 1e-12);  // unchanged
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
  }

  // 2) Hold with no target (cursor in a gap, nothing selected): the whole
  // HoldStart/Rotate/End sequence does nothing — no undo block, no
  // selection churn, no geometry change.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.cursor = 5.0;  // in a gap
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldStart));
    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 8));
    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldEnd));
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
    CHECK(project.selected_calls.empty());
    CHECK(project.items[0].pos == 0.0);
    CHECK(sink.commands.empty());
  }

  // 3) TRIM OUT hold with an existing selection (rule 1): each 4-tick step
  // moves the right edge; the release restores the EXACT selection snapshot
  // (not the target set, not empty).
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // A
    project.items.push_back(ItemInfo{1, 0, 5.0, 2.0, true});   // B selected
    project.cursor = 2.5;
    project.last_touched = 0;
    project.fps = 30.0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_TRIM_OUT, GestureKind::HoldStart));
    r.OnGesture(G(KEY_TRIM_OUT, GestureKind::HoldRotate, 4));  // one step
    CHECK_NEAR(project.items[1].len, 2.0 + 1.0 / 30.0, 1e-12);  // B extended
    CHECK_NEAR(project.items[0].len, 5.0, 1e-12);  // A untouched (not a target)
    CHECK(project.selected_calls.empty());  // selection only restored at HoldEnd

    r.OnGesture(G(KEY_TRIM_OUT, GestureKind::HoldEnd));
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: trim out");
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].size() == 1);
    CHECK(project.selected_calls[0][0] == 1);  // selection restored: B
  }

  // 4) Unmapped hold key (ROLL): HoldStart/Rotate/End all do nothing,
  // even with a target available.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;
    project.last_touched = 0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_ROLL, GestureKind::HoldStart));
    r.OnGesture(G(KEY_ROLL, GestureKind::HoldRotate, 8));
    r.OnGesture(G(KEY_ROLL, GestureKind::HoldEnd));
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
    CHECK(project.selected_calls.empty());
    CHECK(project.items[0].pos == 0.0);
  }

  // 5) Negative ticks: SPLIT hold −4 ticks → pos −1 frame.
  {
    FakeSink sink;
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 2.0, 5.0, false});
    project.cursor = 3.0;
    project.last_touched = 0;
    project.fps = 30.0;
    VerbRouter r(sink, project);

    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldStart));
    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, -4));  // one step back
    CHECK_NEAR(project.items[0].pos, 2.0 - 1.0 / 30.0, 1e-12);
    r.OnGesture(G(KEY_SPLIT, GestureKind::HoldEnd));
    CHECK(project.undo_ends == 1);
  }
}

}  // namespace

int main() {
  test_mapped_gestures();
  test_unmapped_gestures_do_nothing();
  test_hold_rotate();
  test_trim_composites();
  test_ripl_del_composite();
  test_double_tap_keys();
  test_timeline_gestures();
  test_cam_takes();
  test_audio_level_and_smth_cut();
  test_place_on_top();
  test_view_buttons();
  return check_failures();
}
