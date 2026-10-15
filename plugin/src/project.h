// Pure, REAPER-free view of the project STATE that item verbs need:
// state reads plus the smallest
// write primitives. Multi-step actions (duplicate-onto-new-track, view
// toggles, the LED snapshot) are pure ops in project_ops.h built on
// these primitives, so fake_project.h re-implements no verb logic.
// ReaperProject (not in this header's translation unit graph) implements
// IProject against the REAPER API; plugin/test/fake_project.h implements
// it for unit tests.
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

// Item geometry: item-level pos/len/vol plus active-take start offset
// and play rate. A bad id yields {0, 0, 0, 1, 1} (or zeros on the Reaper
// side, mirroring ItemVolume's 0.0).
struct ItemGeom {
  double pos;        // D_POSITION, seconds
  double len;        // D_LENGTH, seconds
  double startoffs;  // active take D_STARTOFFS, seconds
  double playrate;   // active take D_PLAYRATE, 1.0 = normal speed
  double vol;        // D_VOL, linear (1.0 = 0 dB)
  double fadein = 0.0;   // D_FADEINLEN, seconds
  double fadeout = 0.0;  // D_FADEOUTLEN, seconds
};

// Track-level flags the view ops toggle (the primitives behind
// project_ops.h).
enum class TrackFlag : int {
  ShowTcp = 0,    // B_SHOWINTCP (visible in TCP)
  ShowMixer = 1,  // B_SHOWINMIXER (visible in mixer)
  Solo = 2,       // I_SOLO
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
  // Takes. id is an Items() id; a missing id is a no-op /
  // reports 0 takes and no active take (-1).
  virtual int ItemTakeCount(int id) = 0;  // CountTakes
  virtual int ItemActiveTake(int id) = 0;  // 0-based (I_CURTAKE); -1 if none
  virtual void SetItemActiveTake(int id, int take) = 0;  // I_CURTAKE + UpdateItemInProject
  virtual double ItemVolume(int id) = 0;  // D_VOL, linear (1.0 = 0 dB)
  virtual double Fps() = 0;  // TimeMap_curFrameRate; 30.0 if unknown
  // Nudge step sizes (the `step=` arg): the project grid division in
  // seconds at the cursor (<= 0 if unknown) and one sample (project rate).
  virtual double GridSeconds() = 0;
  virtual double SampleSeconds() = 0;
  // Time selection edges. TimeSelection: false when there is none;
  // SetTimeSelection writes start/end (GetSet_LoopTimeRange2).
  virtual bool TimeSelection(double *start, double *end) = 0;
  virtual void SetTimeSelection(double start, double end) = 0;
  // Item geometry. pos/len/vol are item properties (D_POSITION,
  // D_LENGTH, D_VOL); startoffs/playrate come from the active take
  // (D_STARTOFFS, D_PLAYRATE). SetGeom writes all five and calls
  // UpdateItemInProject; a bad id is a no-op.
  virtual ItemGeom GetGeom(int id) = 0;
  virtual void SetGeom(int id, const ItemGeom &g) = 0;
  // Write primitives: the building blocks the pure ops
  // in project_ops.h combine. Track indices are the Items()/track-list
  // index (master excluded); item ids are Items() ids.
  virtual int TrackCount() = 0;  // CountTracks
  virtual bool ToggleState(int cmd) = 0;  // REAPER toggle command on/off
  // Insert one empty track at `track` (above the track currently there);
  // returns the new track's index, -1 on failure.
  virtual int InsertTrackAbove(int track) = 0;
  // Copy item `id` (state chunk + edit position) onto a new item on
  // `track`; returns the new item's id, -1 on a bad id / track.
  virtual int CopyItemToTrack(int item_id, int track) = 0;
  // Track flag read/write; a missing track reports the flag's default
  // (shown / not solo) and is a no-op target for SetTrackFlag.
  virtual bool GetTrackFlag(int track, TrackFlag flag) = 0;
  virtual void SetTrackFlag(int track, TrackFlag flag, bool on) = 0;
};

#endif  // SPEED_REAPER_PROJECT_H
