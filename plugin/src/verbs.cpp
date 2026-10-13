#include "verbs.h"

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
  void (*run)(IActionSink &, IProject &);
};

// TRIM IN / TRIM OUT (M5 design §3): the same ids as before (40511 / 40512),
// now wrapped so they act on the implicit targets and undo in one step.
void TrimIn(IActionSink &sink, IProject &project) {
  project.Undo_Begin();
  WithTargetSelected(project, [&sink] { sink.RunCommand(40511); });
  project.Undo_End("Speed Reaper: trim");
}

void TrimOut(IActionSink &sink, IProject &project) {
  project.Undo_Begin();
  WithTargetSelected(project, [&sink] { sink.RunCommand(40512); });
  project.Undo_End("Speed Reaper: trim");
}

// RIPL DEL (M5 design §3): with a time selection, remove its contents and
// close the gap on all tracks (40201). Else T: remove the target items AND
// close the gap — if ripple is off, switch to per-track (40310), remove
// (40006), restore (40309); else just 40006. No targets → trace
// `target none` and do nothing.
void RiplDel(IActionSink &sink, IProject &project) {
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

constexpr CompositeRow kCompositeTable[] = {
    {KEY_TRIM_IN,  GestureKind::Tap, TrimIn},  // Item: Trim items left of edit cursor
    {KEY_TRIM_OUT, GestureKind::Tap, TrimOut},  // Item: Trim items right of edit cursor
    {KEY_RIPL_DEL, GestureKind::Tap, RiplDel},  // remove targets (+close gap) / time sel
};

// Composite row for a (key, kind) pair, or nullptr when none (the id table
// then decides).
const CompositeRow *CompositeFor(std::uint16_t key, GestureKind kind) {
  for (const auto &row : kCompositeTable)
    if (row.key == key && row.kind == kind) return &row;
  return nullptr;
}

}  // namespace

void VerbRouter::OnGesture(const Gesture &g) {
  if (const CompositeRow *row = CompositeFor(g.key, g.kind)) {
    row->run(sink_, project_);  // composite rows win over id rows
    return;
  }
  const int cmd = CommandFor(g.key, g.kind);
  if (cmd < 0) return;  // unmapped gesture: do nothing
  sink_.RunCommand(cmd);
}

std::vector<std::uint16_t> VerbDoubleTapKeys() {
  return {KEY_IN, KEY_OUT, KEY_ESC};
}
