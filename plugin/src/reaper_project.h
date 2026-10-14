// REAPER-backed IProject (docs/plugin-design-m5-m9.md §2).
//
// Not in PLUGIN_PURE_SOURCES: it calls the REAPER host API, so it is linked
// into the plugin target only (plugin/CMakeLists.txt). main.cpp constructs
// the global g_project and hands it to VerbRouter (M5 step 3).
#ifndef SPEED_REAPER_REAPER_PROJECT_H
#define SPEED_REAPER_REAPER_PROJECT_H

#include <utility>  // std::pair (saved solo flags)

#include "project.h"
#include "reaper_plugin.h"  // ReaProject / MediaTrack / MediaItem declarations

class ReaperProject final : public IProject {
 public:
  // Resolve host APIs that the vendored reaper_plugin_functions.h does not
  // list (GetSet_Loc). Call once from ReaperPluginEntry after
  // REAPERAPI_LoadAPI; methods whose API resolved to nullptr degrade safely
  // (HasTimeSelection reports false — the oracle host lacks the export).

  std::vector<ItemInfo> Items() override;
  std::vector<int> SelectedTracks() override;
  int LastTouchedTrack() override;
  double CursorPos() override;
  bool HasTimeSelection() override;
  void SetSelectedItems(const std::vector<int> &ids) override;
  int RippleMode() override;
  void Undo_Begin() override;
  void Undo_End(const char *label) override;
  int ItemTakeCount(int id) override;
  int ItemActiveTake(int id) override;
  void SetItemActiveTake(int id, int take) override;
  double ItemVolume(int id) override;
  double Fps() override;
  ItemGeom GetGeom(int id) override;
  void SetGeom(int id, const ItemGeom &g) override;
  void DuplicateItemsAbove(const std::vector<int> &ids) override;
  void ToggleIsolateTracks(const std::vector<int> &item_ids) override;
  void ToggleSoloTracks(const std::vector<int> &item_ids) override;

 private:
  struct ShowFlag {
    bool tcp = true;  // B_SHOWINTCP
    bool mixer = true;  // B_SHOWINMIXER
  };
  bool isolate_active_ = false;
  bool solo_active_ = false;
  std::vector<ShowFlag> saved_isolate_;  // all tracks, index-aligned
  std::vector<std::pair<int, int>> saved_solo_;  // (track, previous I_SOLO)
};

#endif  // SPEED_REAPER_REAPER_PROJECT_H
