// Configurable IProject fake for unit tests (docs/plugin-design-m5-m9.md
// §2). Everything is a plain member; SetSelectedItems is recorded (not
// applied) so tests can assert exactly which selections a verb makes.
#ifndef SPEED_REAPER_TEST_FAKE_PROJECT_H
#define SPEED_REAPER_TEST_FAKE_PROJECT_H

#include <string>
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
};

#endif  // SPEED_REAPER_TEST_FAKE_PROJECT_H
