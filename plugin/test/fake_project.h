// Configurable IProject fake for unit tests (docs/plugin-design-m5-m9.md
// §2). Everything is a plain member; SetSelectedItems is recorded (not
// applied) so tests can assert exactly which selections a verb makes.
#ifndef SPEED_REAPER_TEST_FAKE_PROJECT_H
#define SPEED_REAPER_TEST_FAKE_PROJECT_H

#include <algorithm>  // std::find (DuplicateItemsAbove), std::max (view toggles)
#include <string>
#include <utility>
#include <vector>

#include "project.h"

class FakeProject final : public IProject {
 public:
  std::vector<ItemInfo> items;  // keep ordered by track then pos
  std::vector<int> tracks;      // selected tracks
  int last_touched = -1;
  double cursor = 0.0;
  bool time_sel = false;
  int ripple = 0;

  // SetSelectedItems calls in order.
  std::vector<std::vector<int>> selected_calls;
  int undo_begins = 0;
  int undo_ends = 0;
  std::string last_undo_label;

  // Takes: index-aligned with items; a valid id with no entry behaves like
  // a plain 1-take item (count 1, active take 0). An out-of-range id
  // reports 0 takes / no active take (-1); SetItemActiveTake records the
  // call but applies only to valid ids.
  std::vector<int> take_counts;
  std::vector<int> active_takes;
  // SetItemActiveTake calls in order (id, take).
  std::vector<std::pair<int, int>> set_take_calls;

  // Volumes: index-aligned with items; a valid id with no entry is a plain
  // item at default volume (1.0 = 0 dB). An out-of-range id reports 0.0.
  std::vector<double> item_volumes;

  // DuplicateItemsAbove calls in order (the ids passed).
  std::vector<std::vector<int>> duplicate_calls;

  // View buttons (M6): per-track visibility/solo state, index-aligned with
  // track indices. ToggleIsolateTracks / ToggleSoloTracks save the previous
  // flags on the first call and restore exactly those on the second; the
  // active/saved state is plain members (plugin memory only).
  struct TrackView {
    bool show_tcp = true;    // B_SHOWINTCP
    bool show_mixer = true;  // B_SHOWINMIXER
    int solo = 0;            // I_SOLO
  };
  std::vector<TrackView> track_views;
  bool isolate_active = false;
  bool solo_active = false;
  std::vector<TrackView> saved_isolate_;  // all tracks, index-aligned
  std::vector<std::pair<int, int>> saved_solo_;  // (track, previous I_SOLO)
  // Toggle calls in order (the ids passed).
  std::vector<std::vector<int>> isolate_calls;
  std::vector<std::vector<int>> solo_calls;

  std::vector<ItemInfo> Items() override { return items; }
  std::vector<int> SelectedTracks() override { return tracks; }
  int LastTouchedTrack() override { return last_touched; }
  double CursorPos() override { return cursor; }
  bool HasTimeSelection() override { return time_sel; }
  void SetSelectedItems(const std::vector<int> &ids) override {
    selected_calls.push_back(ids);
  }
  int RippleMode() override { return ripple; }
  void Undo_Begin() override { ++undo_begins; }
  void Undo_End(const char *label) override {
    ++undo_ends;
    last_undo_label = label ? label : "";
  }
  int ItemTakeCount(int id) override {
    if (id < 0 || id >= static_cast<int>(items.size())) return 0;
    return id < static_cast<int>(take_counts.size()) ? take_counts[id] : 1;
  }
  int ItemActiveTake(int id) override {
    if (id < 0 || id >= static_cast<int>(items.size())) return -1;
    return id < static_cast<int>(active_takes.size()) ? active_takes[id] : 0;
  }
  void SetItemActiveTake(int id, int take) override {
    set_take_calls.emplace_back(id, take);
    if (id < 0 || id >= static_cast<int>(items.size())) return;
    if (id >= static_cast<int>(active_takes.size()))
      active_takes.resize(id + 1, 0);
    active_takes[id] = take;
  }
  double ItemVolume(int id) override {
    if (id < 0 || id >= static_cast<int>(items.size())) return 0.0;
    return id < static_cast<int>(item_volumes.size()) ? item_volumes[id] : 1.0;
  }
  // Unique track indices of the valid ids, in order of first appearance.
  std::vector<int> TargetTracksOf(const std::vector<int> &item_ids) const {
    std::vector<int> out;
    for (int id : item_ids) {
      if (id < 0 || id >= static_cast<int>(items.size())) continue;
      const int t = items[id].track;
      if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
    }
    return out;
  }
  void ToggleIsolateTracks(const std::vector<int> &item_ids) override {
    isolate_calls.push_back(item_ids);
    if (isolate_active) {  // second call: restore exactly the saved flags
      for (size_t i = 0;
           i < saved_isolate_.size() && i < track_views.size(); ++i)
        track_views[i] = saved_isolate_[i];  // tracks added after the save
                                             // keep their defaults
      saved_isolate_.clear();
      isolate_active = false;
      return;
    }
    const std::vector<int> targets = TargetTracksOf(item_ids);
    if (targets.empty()) return;  // no valid target: no state change
    int max_t = -1;
    for (const int t : targets) max_t = std::max(max_t, t);
    if (max_t + 1 > static_cast<int>(track_views.size()))
      track_views.resize(max_t + 1);
    saved_isolate_ = track_views;  // every track's previous visibility
    for (size_t i = 0; i < track_views.size(); ++i) {
      const bool show =
          std::find(targets.begin(), targets.end(), static_cast<int>(i)) !=
          targets.end();
      track_views[i].show_tcp = show;
      track_views[i].show_mixer = show;
    }
    isolate_active = true;
  }
  void ToggleSoloTracks(const std::vector<int> &item_ids) override {
    solo_calls.push_back(item_ids);
    if (solo_active) {  // second call: restore exactly the saved flags
      for (const auto &p : saved_solo_)
        if (p.first >= 0 && p.first < static_cast<int>(track_views.size()))
          track_views[p.first].solo = p.second;
      saved_solo_.clear();
      solo_active = false;
      return;
    }
    const std::vector<int> targets = TargetTracksOf(item_ids);
    if (targets.empty()) return;  // no valid target: no state change
    int max_t = -1;
    for (const int t : targets) max_t = std::max(max_t, t);
    if (max_t + 1 > static_cast<int>(track_views.size()))
      track_views.resize(max_t + 1);
    saved_solo_.clear();
    for (const int t : targets) {
      saved_solo_.emplace_back(t, track_views[t].solo);
      track_views[t].solo = 1;
    }
    solo_active = true;
  }
  // Simulates the duplication: one new track above each source track
  // (track numbers above a new track shift by one), copies at the same
  // position, unselected; originals and their selection untouched.
  void DuplicateItemsAbove(const std::vector<int> &ids) override {
    duplicate_calls.push_back(ids);
    std::vector<int> tracks;  // unique source tracks, descending
    for (int id : ids) {
      if (id < 0 || id >= static_cast<int>(items.size())) continue;
      const int t = items[id].track;
      if (std::find(tracks.begin(), tracks.end(), t) == tracks.end())
        tracks.push_back(t);
    }
    std::sort(tracks.rbegin(), tracks.rend());
    for (const int t : tracks) {
      // Copies first (membership is captured before the insert).
      std::vector<ItemInfo> copies;
      for (int id : ids) {
        if (id < 0 || id >= static_cast<int>(items.size())) continue;
        const ItemInfo &s = items[id];
        if (s.track != t) continue;
        copies.push_back(s);
        copies.back().selected = false;
      }
      // Insert the new track at index t: everything on/above it shifts.
      for (auto &it : items)
        if (it.track >= t) ++it.track;
      for (auto &c : copies) c.track = t;  // the new track sits at index t
      items.insert(items.end(), copies.begin(), copies.end());
      for (size_t i = 0; i < items.size(); ++i) items[i].id = static_cast<int>(i);
    }
  }
};

#endif  // SPEED_REAPER_TEST_FAKE_PROJECT_H
