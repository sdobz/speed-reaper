// project_ops.cpp — pure project actions on IProject primitives. The
// REAPER side (ReaperProject) supplies the
// primitives; this file owns the sequencing and the saved view state, so
// it runs unchanged against the test fake.
#include "project_ops.h"
#include "reaper_commands.h"

#include <algorithm>  // std::find

namespace {

// Unique track indices of the valid item ids, in order of first
// appearance (the view buttons act on the target items' tracks).
std::vector<int> TargetTracks(const std::vector<ItemInfo> &items,
                              const std::vector<int> &ids) {
  std::vector<int> out;
  for (int id : ids) {
    if (id < 0 || id >= static_cast<int>(items.size())) continue;
    const int t = items[id].track;
    if (t < 0) continue;
    if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
  }
  return out;
}

}  // namespace

void DuplicateItemsAbove(IProject &p, const std::vector<int> &ids) {
  const std::vector<ItemInfo> items = p.Items();  // id == index (snapshot)
  std::vector<int> source_tracks;  // unique tracks of the valid ids
  for (int id : ids) {
    if (id < 0 || id >= static_cast<int>(items.size())) continue;
    const int t = items[id].track;
    if (t < 0) continue;
    if (std::find(source_tracks.begin(), source_tracks.end(), t) ==
        source_tracks.end())
      source_tracks.push_back(t);
  }
  // Descending: an insert at t shifts only tracks ABOVE t, so the smaller
  // indices still to process stay valid (and inserting empty tracks never
  // changes item order, so the snapshot ids stay valid too).
  for (int i = static_cast<int>(source_tracks.size()) - 1; i >= 0; --i) {
    const int new_track = p.InsertTrackAbove(source_tracks[i]);
    if (new_track < 0) continue;
    // ids are in Items() order (track, then pos) → copies land in pos order.
    for (int id : ids) {
      if (id < 0 || id >= static_cast<int>(items.size())) continue;
      if (items[id].track != source_tracks[i]) continue;
      p.CopyItemToTrack(id, new_track);
    }
  }
}

void IsolateView::Toggle(IProject &p, const std::vector<int> &item_ids) {
  if (active_) {  // second call: restore exactly the saved visibility
    for (size_t i = 0; i < saved_.size(); ++i) {
      p.SetTrackFlag(static_cast<int>(i), TrackFlag::ShowTcp, saved_[i].first);
      p.SetTrackFlag(static_cast<int>(i), TrackFlag::ShowMixer,
                     saved_[i].second);
    }
    saved_.clear();
    active_ = false;
    return;
  }
  const std::vector<int> targets = TargetTracks(p.Items(), item_ids);
  if (targets.empty()) return;  // no valid target: no state change
  const int n = p.TrackCount();
  saved_.resize(n);
  for (int i = 0; i < n; ++i)
    saved_[i] = {p.GetTrackFlag(i, TrackFlag::ShowTcp),
                 p.GetTrackFlag(i, TrackFlag::ShowMixer)};
  for (int i = 0; i < n; ++i) {
    const bool show =
        std::find(targets.begin(), targets.end(), i) != targets.end();
    p.SetTrackFlag(i, TrackFlag::ShowTcp, show);
    p.SetTrackFlag(i, TrackFlag::ShowMixer, show);
  }
  active_ = true;
}

void SoloView::Toggle(IProject &p, const std::vector<int> &item_ids) {
  if (active_) {  // second call: restore exactly the saved solo flags
    for (const auto &s : saved_)
      p.SetTrackFlag(s.first, TrackFlag::Solo, s.second != 0);
    saved_.clear();
    active_ = false;
    return;
  }
  const std::vector<int> targets = TargetTracks(p.Items(), item_ids);
  if (targets.empty()) return;  // no valid target: no state change
  saved_.clear();
  for (const int t : targets) {
    saved_.emplace_back(t, p.GetTrackFlag(t, TrackFlag::Solo) ? 1 : 0);
    p.SetTrackFlag(t, TrackFlag::Solo, true);
  }
  active_ = true;
}

LedState ReadLedState(IProject &p, bool isolate, bool solo) {
  LedState s;
  s.snap = p.ToggleState(reaper_cmd::Snap);
  s.ripple = p.RippleMode();
  s.trim_behind = p.ToggleState(reaper_cmd::TrimBehind);
  // active_take: the 1-based active take shared by ALL selected items,
  // else 0 (nothing selected, mixed, no active take, or take > 9 — the
  // main panel has only the CAM 1..9 bits).
  int shared = 0;
  for (const ItemInfo &item : p.Items()) {
    if (!item.selected) continue;
    const int t = p.ItemActiveTake(item.id) + 1;  // 1-based
    if (t < 1 || t > 9) { s.active_take = 0; return s; }
    if (shared == 0)
      shared = t;
    else if (shared != t) { s.active_take = 0; return s; }
  }
  s.active_take = shared;
  s.isolate = isolate;
  s.solo = solo;
  return s;
}
