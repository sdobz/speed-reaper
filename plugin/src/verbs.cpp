#include "verbs.h"

#include "speed_editor/key_map.h"  // KEY_* button codes (verbatim driver copy)

namespace {

// The single verb table (design §7): (key, kind) → REAPER command id.
// Ids are candidates confirmed by behavior via the M3 oracle / hand test;
// a wrong id is a one-line fix here.
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
    {KEY_TRIM_IN,  GestureKind::Tap,       40511},  // Item: Trim items left of edit cursor
    {KEY_TRIM_OUT, GestureKind::Tap,       40512},  // Item: Trim items right of edit cursor
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

}  // namespace

void VerbRouter::OnGesture(const Gesture &g) {
  const int cmd = CommandFor(g.key, g.kind);
  if (cmd < 0) return;  // unmapped gesture: do nothing
  sink_.RunCommand(cmd);
}

std::vector<std::uint16_t> VerbDoubleTapKeys() {
  return {KEY_IN, KEY_OUT, KEY_ESC};
}
