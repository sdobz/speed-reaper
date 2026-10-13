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
  // Takes (M6). id is an Items() id; a missing id is a no-op /
  // reports 0 takes and no active take (-1).
  virtual int ItemTakeCount(int id) = 0;  // CountTakes
  virtual int ItemActiveTake(int id) = 0;  // 0-based (I_CURTAKE); -1 if none
  virtual void SetItemActiveTake(int id, int take) = 0;  // I_CURTAKE + UpdateItemInProject
  virtual double ItemVolume(int id) = 0;  // D_VOL, linear (1.0 = 0 dB)
  // PLACE ON TOP (M6): duplicate the given ids onto one new track above
  // each source track, same position; the originals and their selection
  // are left alone. Bad ids are ignored.
  virtual void DuplicateItemsAbove(const std::vector<int> &ids) = 0;
  // View buttons (M6): toggle isolate (hide every track except the target
  // items' tracks, B_SHOWINTCP/B_SHOWINMIXER) / solo (I_SOLO on the target
  // items' tracks). The first call saves the previous per-track flags and
  // the second restores exactly those; the saved state is plugin memory
  // only (not REAPER undo). Bad ids are ignored; no valid target ids is a
  // no-op that does not toggle the saved state.
  virtual void ToggleIsolateTracks(const std::vector<int> &item_ids) = 0;
  virtual void ToggleSoloTracks(const std::vector<int> &item_ids) = 0;
};

#endif  // SPEED_REAPER_PROJECT_H
