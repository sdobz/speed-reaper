// Implicit-target rule for item verbs (docs/plugin-design-m5-m9.md §1.1):
// the Speed Editor has no select button, so every item verb resolves its
// targets without a selection. The device never silently changes the
// selection: WithTargetSelected snapshots, selects only when there was no
// selection, runs the verb, and restores.
#ifndef SPEED_REAPER_TARGET_H
#define SPEED_REAPER_TARGET_H

#include <vector>

#include "project.h"

// §1.1, in order: (1) selected items, unchanged; (2) the item on the target
// track containing the cursor — pos <= cursor < pos+len, or cursor exactly
// at the end of that track's last item; a boundary between two items belongs
// to the one starting there. Target track = first selected track, else
// last-touched track. (3) Nothing: empty.
std::vector<int> TargetItems(IProject &project);

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
