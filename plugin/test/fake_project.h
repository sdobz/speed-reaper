// Configurable IProject fake for unit tests (docs/plugin-design-m5-m9.md
// §2). Everything is a plain member; SetSelectedItems is recorded (not
// applied) so tests can assert exactly which selections a verb makes.
#ifndef SPEED_REAPER_TEST_FAKE_PROJECT_H
#define SPEED_REAPER_TEST_FAKE_PROJECT_H

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
};

#endif  // SPEED_REAPER_TEST_FAKE_PROJECT_H
