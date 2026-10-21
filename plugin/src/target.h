// Implicit-target rule for item verbs:
// the Speed Editor has no select button, so every item verb resolves its
// targets without a selection. The device never silently changes the
// selection: WithTargetSelected snapshots, selects only when there was no
// selection, runs the verb, and restores.
#ifndef SPEED_REAPER_TARGET_H
#define SPEED_REAPER_TARGET_H

#include <vector>

#include "project.h"

// The rule, in order: (1) selected items, unchanged; (2) the item on the target
// track containing the cursor — pos <= cursor < pos+len, or cursor exactly
// at the end of that track's last item; a boundary between two items belongs
// to the one starting there. Target track = first selected track, else
// last-touched track. (3) Nothing: empty.
std::vector<int> TargetItems(IProject &project);

// Target track = first selected track, else last-touched track; -1 if none.
int TargetTrack(IProject &project);

// Containment rule for one track only: the item with pos <= cursor <
// pos+len; a boundary between two items belongs to the one starting there;
// the cursor exactly at the track's last item end belongs to that item.
// items must be ordered by track then pos. Item id, or -1 if none match.
int CursorItemOnTrack(const std::vector<ItemInfo> &items, int track,
                      double cursor);

// Rule 2 only (for the explicit TIMELINE select verbs):
// the item on the target track containing the cursor, ignoring any existing
// selection. 0 or 1 element.
std::vector<int> CursorTargetItem(IProject &project);

// Snapshot the selection; if (and only if) nothing is selected but targets
// exist, select the targets; call f; restore the snapshot. Net selection
// change = none.
template <class F> void WithTargetSelected(IProject &project, F f) {
  std::vector<int> original;
  for (const ItemInfo &item : project.Items())
    if (item.selected) original.push_back(item.id);
  const std::vector<int> targets = TargetItems(project);
  const bool selecting = original.empty() && !targets.empty();
  if (selecting) project.SetSelectedItems(targets);
  f();
  project.SetSelectedItems(original);
}

#endif  // SPEED_REAPER_TARGET_H
