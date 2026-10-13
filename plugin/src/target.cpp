// Implicit-target rule (docs/plugin-design-m5-m9.md §1.1). See target.h.
#include "target.h"

#include <cmath>

namespace {

constexpr double kEps = 1e-9;  // item ends and the cursor are doubles in seconds

}  // namespace

// Target track = first selected track, else last-touched track; -1 if none.
int TargetTrack(IProject &project) {
  const std::vector<int> tracks = project.SelectedTracks();
  if (!tracks.empty()) return tracks.front();
  return project.LastTouchedTrack();
}

std::vector<int> CursorTargetItem(IProject &project) {
  const int track = TargetTrack(project);
  if (track < 0) return {};
  const double cursor = project.CursorPos();

  int last_item_id = -1;
  double last_end = 0.0;
  for (const ItemInfo &item : project.Items()) {  // ordered by track then pos
    if (item.track != track) continue;
    if (item.pos <= cursor && cursor < item.pos + item.len)
      return {item.id};  // boundary: only the item starting there contains
                         // the cursor, since items on a track are in pos order
    last_item_id = item.id;
    last_end = item.pos + item.len;
  }
  // At the very end of the track's last item: that item.
  if (last_item_id >= 0 && std::fabs(cursor - last_end) < kEps)
    return {last_item_id};
  return {};
}

std::vector<int> TargetItems(IProject &project) {
  // Rule 1: a selection exists → the selected items (REAPER-native).
  std::vector<int> selected;
  for (const ItemInfo &item : project.Items())
    if (item.selected) selected.push_back(item.id);
  if (!selected.empty()) return selected;

  // Rule 2, then rule 3 (empty): the cursor item on the target track.
  return CursorTargetItem(project);
}
