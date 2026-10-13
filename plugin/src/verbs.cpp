#include "verbs.h"

#include <algorithm>  // std::clamp (TIMELINE select stepping)
#include <cmath>      // std::log10 (AUDIO LEVEL read-out)
#include <cstdio>     // std::snprintf (CAM undo labels, AUDIO LEVEL message)

#include "speed_editor/key_map.h"  // KEY_* button codes (verbatim driver copy)
#include "target.h"                // TargetItems, WithTargetSelected (M5 design §1.1)
#include "trace.h"                 // Trace (no-target diagnostics)

namespace {

// The verb table (design §7): (key, kind) → REAPER command id.
// Ids are candidates confirmed by behavior via the M3 oracle / hand test;
// a wrong id is a one-line fix here. (M5 design §3: TRIM IN / TRIM OUT
// live in the composite table kCompositeTable, which wins over id rows.)
struct VerbRow {
  std::uint16_t key;
  GestureKind kind;
  int command;
};

constexpr VerbRow kVerbTable[] = {
    {KEY_SPLIT,    GestureKind::Tap,       40012},  // Item: Split items at edit cursor
    {KEY_IN,       GestureKind::Tap,       40625},  // Time selection: Set start point
    {KEY_OUT,      GestureKind::Tap,       40626},  // Time selection: Set end point
    {KEY_IN,       GestureKind::DoubleTap, 40635},  // Time selection: Remove (unselect) time selection
    {KEY_OUT,      GestureKind::DoubleTap, 40635},  // Time selection: Remove (unselect) time selection
    {KEY_SNAP,     GestureKind::Tap,       1157},   // Options: Toggle snapping
    {KEY_ESC,      GestureKind::Tap,       40635},  // Time selection: Remove (unselect) time selection
    {KEY_ESC,      GestureKind::DoubleTap, 40029},  // Edit: Undo
    {KEY_AUDIO_LEVEL, GestureKind::DoubleTap, 40157},  // Insert: Insert marker at edit cursor
    {KEY_FULL_VIEW, GestureKind::Tap, 40346},  // View: toggle maximized arrange (candidate, settled by M6 task 6)
};

// Command id for a (key, kind) pair, or -1 when the gesture is unmapped
// (including every Hold* kind: no hold verbs in M3).
int CommandFor(std::uint16_t key, GestureKind kind) {
  for (const auto &row : kVerbTable)
    if (row.key == key && row.kind == kind) return row.command;
  return -1;
}

// Composite verbs (M5 design §2): function-pointer rows next to the
// command-id table; a composite row wins over an id row for the same
// (key, kind). M5 §3 "T" verbs: run inside WithTargetSelected plus one
// undo block, so the implicit-target selection never sticks.
struct CompositeRow {
  std::uint16_t key;
  GestureKind kind;
  void (*run)(VerbRouter &, IActionSink &, IProject &, const Gesture &);
};

// TRIM IN / TRIM OUT (M5 design §3): the same ids as before (40511 / 40512),
// now wrapped so they act on the implicit targets and undo in one step.
void TrimIn(VerbRouter &, IActionSink &sink, IProject &project, const Gesture &) {
  project.Undo_Begin();
  WithTargetSelected(project, [&sink] { sink.RunCommand(40511); });
  project.Undo_End("Speed Reaper: trim");
}

void TrimOut(VerbRouter &, IActionSink &sink, IProject &project, const Gesture &) {
  project.Undo_Begin();
  WithTargetSelected(project, [&sink] { sink.RunCommand(40512); });
  project.Undo_End("Speed Reaper: trim");
}

// RIPL DEL (M5 design §3): with a time selection, remove its contents and
// close the gap on all tracks (40201). Else T: remove the target items AND
// close the gap — if ripple is off, switch to per-track (40310), remove
// (40006), restore (40309); else just 40006. No targets → trace
// `target none` and do nothing.
void RiplDel(VerbRouter &, IActionSink &sink, IProject &project, const Gesture &) {
  if (project.HasTimeSelection()) {
    sink.RunCommand(40201);
    return;
  }
  if (TargetItems(project).empty()) {
    Trace("target none");  // §1.1 rule 3: the verb does nothing
    return;
  }
  const bool ripple_off = project.RippleMode() == 0;
  project.Undo_Begin();
  WithTargetSelected(project, [&sink, ripple_off] {
    if (ripple_off) sink.RunCommand(40310);  // Options: Toggle ripple edit (per-track)
    sink.RunCommand(40006);                  // Item: Remove items
    if (ripple_off) sink.RunCommand(40309);  // restore ripple: off
  });
  project.Undo_End("Speed Reaper: delete");
}

constexpr int kSelectStepTicks = 20;  // wheel ticks per TIMELINE select step

// TIMELINE select verbs (M5 design §1.2): explicit selection — unlike the
// implicit "T" verbs, these deliberately change the selection.
void TimelineTap(VerbRouter &, IActionSink &, IProject &project, const Gesture &) {
  const std::vector<int> target = CursorTargetItem(project);
  if (target.empty()) {
    Trace("target none");
    return;
  }
  std::vector<int> selected;
  for (const ItemInfo &item : project.Items())
    if (item.selected) selected.push_back(item.id);
  if (selected.size() == 1 && selected[0] == target[0])
    project.SetSelectedItems({});  // sole selection → tap again = deselect all
  else
    project.SetSelectedItems(target);  // select the target exclusively
}

void TimelineDoubleTap(VerbRouter &, IActionSink &sink, IProject &, const Gesture &) {
  sink.RunCommand(40289);  // deselect all items
}

// CAM 1–9 (M6 design §3): set the active take N (I_CURTAKE = N-1) on each
// target item; items with fewer than N takes keep theirs. N is derived from
// the key (KEY_CAM1..KEY_CAM9 are contiguous), so one function serves all
// nine rows. No targets → trace `target none` and do nothing.
void CamSetTake(VerbRouter &, IActionSink &, IProject &project, const Gesture &g) {
  const int n = static_cast<int>(g.key - KEY_CAM1) + 1;  // 1..9
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");  // §1.1 rule 3: the verb does nothing
    return;
  }
  project.Undo_Begin();
  WithTargetSelected(project, [&project, &targets, n] {
    for (const int id : targets)
      if (project.ItemTakeCount(id) >= n)  // fewer than N takes → keep theirs
        project.SetItemActiveTake(id, n - 1);
  });
  char label[32];
  std::snprintf(label, sizeof label, "Speed Reaper: take %d", n);
  project.Undo_End(label);
}

// AUDIO LEVEL Tap (M6 design §3): read out the first target item's volume
// as `Volume: <x.x> dB` (20*log10(D_VOL)) via IActionSink::Message
// (REAPER: ShowConsoleMsg). No targets → trace `target none` and do
// nothing. Selection untouched (pure read-out, no undo block).
void AudioLevelTap(VerbRouter &, IActionSink &sink, IProject &project,
                   const Gesture &) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");
    return;
  }
  char msg[64];
  std::snprintf(msg, sizeof msg, "Volume: %.1f dB",
                20.0 * std::log10(project.ItemVolume(targets.front())));
  sink.Message(msg);
}

// PLACE ON TOP Tap (M6 design §3): duplicate the target items onto one new
// track above each source track, same position (IProject::
// DuplicateItemsAbove); the original selection is restored. No targets →
// trace `target none` and do nothing.
void PlaceOnTopTap(VerbRouter &, IActionSink &, IProject &project,
                   const Gesture &) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");  // §1.1 rule 3: the verb does nothing
    return;
  }
  project.Undo_Begin();
  WithTargetSelected(project, [&project, &targets] {
    project.DuplicateItemsAbove(targets);
  });
  project.Undo_End("Speed Reaper: place on top");
}

// SMTH CUT Tap (M6 design §3): 40916, but only with a time selection —
// the verb has nothing to smooth-cut without one, so trace `no time
// selection` and do nothing.
void SmthCutTap(VerbRouter &, IActionSink &sink, IProject &project,
                const Gesture &) {
  if (project.HasTimeSelection()) {
    sink.RunCommand(40916);
    return;
  }
  Trace("no time selection");
}

// VIDEO_ONLY Tap (M6 design §3): toggle the isolate view — hide every track
// except the target items' tracks; the second tap restores the saved
// per-track visibility (the saved state is plugin memory only, so no undo
// block). No targets → trace `target none` and do nothing.
void VideoOnlyTap(VerbRouter &, IActionSink &, IProject &project,
                  const Gesture &) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");  // §1.1 rule 3: the verb does nothing
    return;
  }
  project.ToggleIsolateTracks(targets);
}

// AUDIO_ONLY Tap (M6 design §3): toggle solo on the target items' tracks;
// the second tap restores the saved solo flags.
void AudioOnlyTap(VerbRouter &, IActionSink &, IProject &project,
                  const Gesture &) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");  // §1.1 rule 3: the verb does nothing
    return;
  }
  project.ToggleSoloTracks(targets);
}

// One hold+rotate step: move the selection to the next (dir>0) / previous
// (dir<0) item on the target track, clamped at the ends; the cursor does
// not move. No selection on the track: + selects the first item, - the
// last. Exclusive selection of the single stepped-to item.
void StepSelection(IProject &project, int dir) {
  const int track = TargetTrack(project);
  if (track < 0) return;
  std::vector<int> ids;  // target-track item ids in pos order
  int sel_idx = -1;      // first (leftmost) selected item on the track, or -1
  for (const ItemInfo &item : project.Items()) {  // ordered by track then pos
    if (item.track != track) continue;
    if (item.selected && sel_idx < 0) sel_idx = static_cast<int>(ids.size());
    ids.push_back(item.id);
  }
  if (ids.empty()) return;
  const int last = static_cast<int>(ids.size()) - 1;
  if (sel_idx < 0) {  // nothing selected on the track: step from an end
    project.SetSelectedItems({ids[dir > 0 ? 0 : last]});
    return;
  }
  const int idx = std::clamp(sel_idx + dir, 0, last);
  project.SetSelectedItems({ids[idx]});
}

void TimelineHoldRotate(VerbRouter &router, IActionSink &,
                        IProject &project, const Gesture &g) {
  router.select_delta_ += g.delta;
  while (router.select_delta_ >= kSelectStepTicks ||
         router.select_delta_ <= -kSelectStepTicks) {
    const int dir = router.select_delta_ > 0 ? 1 : -1;
    router.select_delta_ -= dir * kSelectStepTicks;  // keep the remainder
    StepSelection(project, dir);
  }
}

// The composite rows (M5 design §2/§3), built at first use: the M5 verbs
// plus CAM 1–9 generated as a loop — the keys are contiguous
// (KEY_CAM1..KEY_CAM9) and CamSetTake derives the take number from the key,
// so no nine-row copy-paste.
const std::vector<CompositeRow> &CompositeRows() {
  static const std::vector<CompositeRow> rows = [] {
    std::vector<CompositeRow> r;
    r.push_back({KEY_TRIM_IN,  GestureKind::Tap, TrimIn});  // trim left of cursor
    r.push_back({KEY_TRIM_OUT, GestureKind::Tap, TrimOut});  // trim right of cursor
    r.push_back({KEY_RIPL_DEL, GestureKind::Tap, RiplDel});  // remove targets / time sel
    r.push_back({KEY_TIMELINE, GestureKind::Tap, TimelineTap});  // select cursor target
    r.push_back({KEY_TIMELINE, GestureKind::DoubleTap, TimelineDoubleTap});  // deselect all
    r.push_back({KEY_TIMELINE, GestureKind::HoldRotate, TimelineHoldRotate});  // step selection
    for (int n = 1; n <= 9; ++n)
      r.push_back({static_cast<std::uint16_t>(KEY_CAM1 + n - 1), GestureKind::Tap,
                   CamSetTake});  // M6: active take N on the targets
    r.push_back({KEY_AUDIO_LEVEL, GestureKind::Tap, AudioLevelTap});  // M6: volume read-out
    r.push_back({KEY_SMTH_CUT, GestureKind::Tap, SmthCutTap});  // M6: 40916 if time sel
    r.push_back({KEY_PLACE_ON_TOP, GestureKind::Tap, PlaceOnTopTap});  // M6: duplicate onto a new track above
    r.push_back({KEY_VIDEO_ONLY, GestureKind::Tap, VideoOnlyTap});  // M6: toggle isolate view
    r.push_back({KEY_AUDIO_ONLY, GestureKind::Tap, AudioOnlyTap});  // M6: toggle solo on target tracks
    return r;
  }();
  return rows;
}

// Composite row for a (key, kind) pair, or nullptr when none (the id table
// then decides).
const CompositeRow *CompositeFor(std::uint16_t key, GestureKind kind) {
  for (const auto &row : CompositeRows())
    if (row.key == key && row.kind == kind) return &row;
  return nullptr;
}

}  // namespace

void VerbRouter::OnGesture(const Gesture &g) {
  if (const CompositeRow *row = CompositeFor(g.key, g.kind)) {
    row->run(*this, sink_, project_, g);  // composite rows win over id rows
    return;
  }
  const int cmd = CommandFor(g.key, g.kind);
  if (cmd < 0) return;  // unmapped gesture: do nothing
  sink_.RunCommand(cmd);
}

std::vector<std::uint16_t> VerbDoubleTapKeys() {
  return {KEY_IN, KEY_OUT, KEY_ESC, KEY_TIMELINE, KEY_AUDIO_LEVEL};
}
