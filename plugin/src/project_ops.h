// project_ops.h — the multi-step project actions as PURE functions /
// small state holders built on IProject primitives.
//
// Before this split, IProject itself exposed these verbs, and the test
// fake had to re-implement each one's full logic. Now IProject is state
// plus primitives only, and these ops own the sequencing — including the
// saved previous state the second tap of a view toggle restores.
//
// Pure: no REAPER API, no clock. IProject is the only seam.
#ifndef SPEED_REAPER_PROJECT_OPS_H
#define SPEED_REAPER_PROJECT_OPS_H

#include <utility>
#include <vector>

#include "project.h"    // IProject, TrackFlag

// PLACE ON TOP: duplicate the given ids onto one new track
// above each source track, same position; the originals and their
// selection are left alone. Bad ids are ignored.
void DuplicateItemsAbove(IProject &p, const std::vector<int> &ids);

// Isolate view (VIDEO_ONLY): hide every track except the target
// items' tracks (B_SHOWINTCP/B_SHOWINMIXER). The first Toggle saves the
// previous per-track flags and the second restores exactly those; the
// saved state lives HERE (plugin memory only, not REAPER undo). Bad ids
// are ignored; no valid target ids is a no-op that does not toggle the
// saved state.
class IsolateView {
 public:
  void Toggle(IProject &p, const std::vector<int> &item_ids);
  bool IsActive() const { return active_; }

 private:
  bool active_ = false;
  // (tcp, mixer) per track, index-aligned, captured on the activating
  // Toggle.
  std::vector<std::pair<bool, bool>> saved_;
};

// Solo (AUDIO_ONLY): I_SOLO on the target items' tracks. Same
// save/restore contract as IsolateView (only the target tracks' previous
// flags are saved).
class SoloView {
 public:
  void Toggle(IProject &p, const std::vector<int> &item_ids);
  bool IsActive() const { return active_; }

 private:
  bool active_ = false;
  std::vector<std::pair<int, int>> saved_;  // (track, previous I_SOLO)
};

// The 1-based active take shared by ALL selected items (I_CURTAKE only).
// 0 when nothing is selected, a selected item has no active take, the
// selected items disagree, or the shared take is > 9 (the main panel has
// only the CAM 1..9 bits).
int SharedActiveTake(IProject &p);

#endif  // SPEED_REAPER_PROJECT_OPS_H
