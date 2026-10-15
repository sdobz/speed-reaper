// Configurable IProject fake for unit tests (docs/plugin-design-m5-m9.md
// §2). Primitives only (40-cleanup §8): every member is state, and the
// write primitives apply it directly — the multi-step actions are the
// pure ops in plugin/src/project_ops.h, which run against this fake
// unmodified. SetSelectedItems is recorded (not applied) so tests can
// assert exactly which selections a verb makes.
#ifndef SPEED_REAPER_TEST_FAKE_PROJECT_H
#define SPEED_REAPER_TEST_FAKE_PROJECT_H

#include <algorithm>  // std::max (TrackCount)
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

  // LED bar (M8): snap / trim-behind mirror the REAPER toggles the
  // ReadLedState op queries (ToggleState 1157 / 41117); ripple comes from
  // the `ripple` member above; active_take from the selection +
  // active_takes.
  bool snap = false;
  bool trim_behind = false;

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

  // Geometry (M7): index-aligned with items; a valid id with no entry has
  // plain values (pos/len from items[id], startoffs 0, playrate 1.0, vol
  // 1.0). SetGeom writes both geoms[id] and items[id].pos/len; an
  // out-of-range id is a no-op.
  std::vector<ItemGeom> geoms;
  double fps = 30.0;

  // Write-primitive calls, in order (tests assert the ops combined them
  // the right way).
  std::vector<int> insert_calls;             // InsertTrackAbove(track)
  std::vector<std::pair<int, int>> copy_calls;  // CopyItemToTrack(id, track)

  // Track-flag state, index-aligned with track indices. GetTrackFlag past
  // the end reports the flag's default (shown / not solo), as REAPER does
  // for a missing track; SetTrackFlag grows the vector as needed.
  struct TrackView {
    bool show_tcp = true;    // B_SHOWINTCP
    bool show_mixer = true;  // B_SHOWINMIXER
    int solo = 0;            // I_SOLO
  };
  std::vector<TrackView> track_views;

  std::vector<ItemInfo> Items() override { return items; }
  std::vector<int> SelectedTracks() override { return tracks; }
  int LastTouchedTrack() override { return last_touched; }
  double CursorPos() override { return cursor; }
  bool HasTimeSelection() override { return time_sel; }
  void SetSelectedItems(const std::vector<int> &ids) override {
    selected_calls.push_back(ids);
  }
  int RippleMode() override { return ripple; }
  int TrackCount() override {
    int n = static_cast<int>(track_views.size());
    for (const ItemInfo &it : items) n = std::max(n, it.track + 1);
    return n;
  }
  bool ToggleState(int cmd) override {
    if (cmd == 1157) return snap;
    if (cmd == 41117) return trim_behind;
    return false;
  }
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
  double Fps() override { return fps; }
  ItemGeom GetGeom(int id) override {
    ItemGeom g{0.0, 0.0, 0.0, 1.0, 1.0};  // default for a valid id, no entry
    if (id < 0 || id >= static_cast<int>(items.size())) return g;
    g.pos = items[id].pos;
    g.len = items[id].len;
    if (id < static_cast<int>(geoms.size())) {
      g.startoffs = geoms[id].startoffs;
      g.playrate = geoms[id].playrate;
      g.vol = geoms[id].vol;
    }
    return g;
  }
  void SetGeom(int id, const ItemGeom &g) override {
    if (id < 0 || id >= static_cast<int>(items.size())) return;
    if (id >= static_cast<int>(geoms.size())) geoms.resize(id + 1);
    geoms[id] = g;
    items[id].pos = g.pos;
    items[id].len = g.len;
  }
  // Insert an empty track at `track`: items on/above it shift down by one
  // (their ids — vector indices — are unchanged).
  int InsertTrackAbove(int track) override {
    insert_calls.push_back(track);
    for (auto &it : items)
      if (it.track >= track) ++it.track;
    return track;
  }
  // Copy item `id` onto `track` (same position, unselected — the fake's
  // model of the state-chunk copy); appends to items, renumbers ids.
  int CopyItemToTrack(int item_id, int track) override {
    copy_calls.emplace_back(item_id, track);
    if (item_id < 0 || item_id >= static_cast<int>(items.size())) return -1;
    ItemInfo c = items[item_id];
    c.selected = false;
    c.track = track;
    items.push_back(c);
    for (size_t i = 0; i < items.size(); ++i)
      items[i].id = static_cast<int>(i);
    return static_cast<int>(items.size()) - 1;
  }
  bool GetTrackFlag(int track, TrackFlag flag) override {
    if (track < 0 || track >= static_cast<int>(track_views.size()))
      return flag == TrackFlag::Solo ? false : true;  // the flag's default
    const TrackView &v = track_views[track];
    switch (flag) {
      case TrackFlag::ShowTcp: return v.show_tcp;
      case TrackFlag::ShowMixer: return v.show_mixer;
      case TrackFlag::Solo: return v.solo != 0;
    }
    return false;
  }
  void SetTrackFlag(int track, TrackFlag flag, bool on) override {
    if (track < 0) return;
    if (track >= static_cast<int>(track_views.size()))
      track_views.resize(track + 1);
    TrackView &v = track_views[track];
    switch (flag) {
      case TrackFlag::ShowTcp: v.show_tcp = on; break;
      case TrackFlag::ShowMixer: v.show_mixer = on; break;
      case TrackFlag::Solo: v.solo = on ? 1 : 0; break;
    }
  }
};

#endif  // SPEED_REAPER_TEST_FAKE_PROJECT_H
