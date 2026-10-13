#include "verbs.h"

#include "speed_editor/key_map.h"  // KEY_* button codes (verbatim driver copy)
#include "target.h"                // WithTargetSelected (M5 design §1.1)

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
    {KEY_RIPL_DEL, GestureKind::Tap,       40006},  // Item: Remove items
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

constexpr CompositeRow kCompositeTable[] = {
    {KEY_TRIM_IN,  GestureKind::Tap, TrimIn},  // Item: Trim items left of edit cursor
    {KEY_TRIM_OUT, GestureKind::Tap, TrimOut},  // Item: Trim items right of edit cursor
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
