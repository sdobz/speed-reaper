// Pure, REAPER-free view of the project state that item verbs need
// (docs/plugin-design-m5-m9.md §2). ReaperProject (not in this header's
// translation unit graph) implements IProject against the REAPER API;
// plugin/test/fake_project.h implements it for unit tests.
#ifndef SPEED_REAPER_PROJECT_H
#define SPEED_REAPER_PROJECT_H

#include <vector>

struct ItemInfo {
  int id;       // index in Items() order
  int track;    // track index
  double pos;   // seconds
  double len;   // seconds
  bool selected;
};

class IProject {
 public:
  virtual ~IProject() = default;
  virtual std::vector<ItemInfo> Items() = 0;  // all items, ordered by track then pos
  virtual std::vector<int> SelectedTracks() = 0;  // track indices
  virtual int LastTouchedTrack() = 0;  // -1 if none
  virtual double CursorPos() = 0;
  virtual bool HasTimeSelection() = 0;
  virtual void SetSelectedItems(const std::vector<int> &ids) = 0;
  virtual int RippleMode() = 0;  // 0 off, 1 per-track, 2 all
  virtual void Undo_Begin() = 0;  // Undo_BeginBlock2
  virtual void Undo_End(const char *label) = 0;  // Undo_EndBlock2 (one step)
};

#endif  // SPEED_REAPER_PROJECT_H
