// REAPER-backed IProject.
//
// Not in PLUGIN_PURE_SOURCES: it calls the REAPER host API, so it is linked
// into the plugin target only (plugin/CMakeLists.txt). main.cpp constructs
// the global g_project and hands it to VerbRouter.
//
// This class implements STATE and primitives only. The
// multi-step verbs (duplicate-onto-new-track, isolate/solo toggles, the LED
// snapshot) are pure ops in project_ops.h built on these primitives, so
// their saved-state and sequencing never live here.
#ifndef SPEED_REAPER_REAPER_PROJECT_H
#define SPEED_REAPER_REAPER_PROJECT_H

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
  double PixelsPerSecond() override;  // GetHZoomLevel
  ViewRange GetView() override;       // GetSet_ArrangeView2 read
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
  double GridSeconds() override;
  double SampleSeconds() override;
  bool TimeSelection(double *start, double *end) override;
  void SetTimeSelection(double start, double end) override;
  ItemGeom GetGeom(int id) override;
  void SetGeom(int id, const ItemGeom &g) override;
  int TrackCount() override;
  bool ToggleState(int cmd) override;
  int InsertTrackAbove(int track) override;
  int CopyItemToTrack(int item_id, int track) override;
  bool GetTrackFlag(int track, TrackFlag flag) override;
  void SetTrackFlag(int track, TrackFlag flag, bool on) override;
};

#endif  // SPEED_REAPER_REAPER_PROJECT_H
