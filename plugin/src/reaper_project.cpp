// ReaperProject: IProject over the REAPER host API
// (docs/plugin-design-m5-m9.md §2). API signatures: docs/reaper-api-subset.md
// §2 (cursor), §8 (tracks & media items), §9 (undo).
// SDK/Swell headers: their min/max macros break <algorithm>/<set>
// (same gotcha as main.cpp — include STL before the macros land).
#include <algorithm>
#include <set>
#include "reaper_project.h"

// REAPERAPI_WANT for every listed host API this TU calls. REAPERAPI_MINIMAL
// here makes a missing mirror in main.cpp's list (which owns
// REAPERAPI_IMPLEMENT) a link error on REAPERAPI_<name> — keep both lists in
// sync.
#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_CountMediaItems
#define REAPERAPI_WANT_GetMediaItem
#define REAPERAPI_WANT_GetMediaItem_Track
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_IsMediaItemSelected
#define REAPERAPI_WANT_SetMediaItemSelected
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_GetMediaTrackInfo_Value
#define REAPERAPI_WANT_GetLastTouchedTrack
#define REAPERAPI_WANT_Undo_BeginBlock2
#define REAPERAPI_WANT_Undo_EndBlock2
#define REAPERAPI_WANT_GetCursorPosition
#define REAPERAPI_WANT_GetToggleCommandState
#define REAPERAPI_WANT_GetSet_LoopTimeRange2 // ReaperProject::HasTimeSelection
#define REAPERAPI_WANT_CountTakes // ReaperProject::ItemTakeCount
#define REAPERAPI_WANT_SetMediaItemInfo_Value // ReaperProject::SetItemActiveTake
#define REAPERAPI_WANT_UpdateItemInProject // ReaperProject::SetItemActiveTake
#define REAPERAPI_WANT_InsertTrackInProject // ReaperProject::DuplicateItemsAbove
#define REAPERAPI_WANT_AddMediaItemToTrack // ReaperProject::DuplicateItemsAbove
#define REAPERAPI_WANT_GetItemStateChunk // ReaperProject::DuplicateItemsAbove
#define REAPERAPI_WANT_SetItemStateChunk // ReaperProject::DuplicateItemsAbove
#define REAPERAPI_WANT_SetMediaTrackInfo_Value // ReaperProject::Toggle*Tracks
#define REAPERAPI_WANT_TimeMap_curFrameRate // ReaperProject::Fps
#define REAPERAPI_WANT_GetActiveTake // ReaperProject::GetGeom/SetGeom
#define REAPERAPI_WANT_GetMediaItemTakeInfo_Value // ReaperProject::GetGeom
#define REAPERAPI_WANT_SetMediaItemTakeInfo_Value // ReaperProject::SetGeom
#include "reaper_plugin_functions.h"

namespace {

// Ripple commands (design §3): 40310 = "Set ripple editing per-track",
// 40311 = "Set ripple editing all tracks" (names: docs/reaper-actions.tsv).
constexpr int kRipplePerTrack = 40310;
constexpr int kRippleAll = 40311;
// LED bar (M8, docs/notes/m8-state-reads.md): snap (1157) and trim-behind
// (41117, "Options: Trim content behind media items when editing") are
// plain toggles; all three are valid GetToggleCommandState queries.

// Track-list index of a track (master excluded); -1 if null / not found.
int TrackIndex(MediaTrack *track) {
  if (!track) return -1;
  const int n = CountTracks(nullptr);
  for (int i = 0; i < n; i++)
    if (GetTrack(nullptr, i) == track) return i;
  return -1;
}

// All items in design order (track, then position) — Items() and
// SetSelectedItems() must agree on id == index in this order.
std::vector<MediaItem *> EnumerateItems() {
  struct Entry {
    MediaItem *item;
    int track;
    double pos;
  };
  std::vector<Entry> entries;
  int n = CountMediaItems(nullptr);
  if (n < 0) n = 0;  // the host-layer stub APIs (plugin_host) return junk in the
                     // return register; real REAPER never counts < 0
  entries.reserve(n);
  for (int i = 0; i < n; i++) {
    MediaItem *item = GetMediaItem(nullptr, i);
    if (!item) continue;
    entries.push_back(
        {item, TrackIndex(GetMediaItem_Track(item)),
         GetMediaItemInfo_Value(item, "D_POSITION")});
  }
  std::stable_sort(entries.begin(), entries.end(),
                   [](const Entry &a, const Entry &b) {
                     if (a.track != b.track) return a.track < b.track;
                     return a.pos < b.pos;
                   });
  std::vector<MediaItem *> out;
  out.reserve(entries.size());
  for (const Entry &e : entries) out.push_back(e.item);
  return out;
}

// MediaItem for a design id (index in EnumerateItems() order); null if the
// id is out of range.
MediaItem *ItemById(int id) {
  const std::vector<MediaItem *> items = EnumerateItems();
  if (id < 0 || id >= static_cast<int>(items.size())) return nullptr;
  return items[id];
}

// Unique track indices of the valid item ids, in order of first appearance
// (the view-button verbs act on the target items' tracks).
std::vector<int> TracksOfItems(const std::vector<MediaItem *> &items,
                               const std::vector<int> &ids) {
  std::vector<int> out;
  for (int id : ids) {
    if (id < 0 || id >= static_cast<int>(items.size())) continue;
    const int t = TrackIndex(GetMediaItem_Track(items[id]));
    if (t < 0) continue;
    if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
  }
  return out;
}

}  // namespace

std::vector<ItemInfo> ReaperProject::Items() {
  const std::vector<MediaItem *> items = EnumerateItems();
  std::vector<ItemInfo> out;
  out.reserve(items.size());
  for (size_t i = 0; i < items.size(); i++) {
    MediaItem *item = items[i];
    ItemInfo info;
    info.id = static_cast<int>(i);  // index in Items() order (design §2)
    info.track = TrackIndex(GetMediaItem_Track(item));
    info.pos = GetMediaItemInfo_Value(item, "D_POSITION");
    info.len = GetMediaItemInfo_Value(item, "D_LENGTH");
    info.selected = IsMediaItemSelected(item);
    out.push_back(info);
  }
  return out;
}

std::vector<int> ReaperProject::SelectedTracks() {
  std::vector<int> out;
  const int n = CountTracks(nullptr);
  for (int i = 0; i < n; i++) {
    MediaTrack *tr = GetTrack(nullptr, i);
    if (tr && GetMediaTrackInfo_Value(tr, "B_SELECTED"))
      out.push_back(i);
  }
  return out;
}

int ReaperProject::LastTouchedTrack() {
  return TrackIndex(GetLastTouchedTrack());  // SDK: no proj parameter
}

double ReaperProject::CursorPos() { return GetCursorPosition(); }

bool ReaperProject::HasTimeSelection() {
  // GetSet_LoopTimeRange2 isSet=false reads the current time selection into
  // *start/*end (docs/notes/m5-observations.md: the "GetSet_Loc" name does
  // not exist — real REAPER 7.81 exports GetSet_LoopTimeRange2 instead).
  double t0 = 0.0, t1 = 0.0;
  GetSet_LoopTimeRange2(nullptr, false, false, &t0, &t1, false);
  return t1 > t0;
}

void ReaperProject::SetSelectedItems(const std::vector<int> &ids) {
  const std::vector<MediaItem *> items = EnumerateItems();  // id == index
  std::set<int> want(ids.begin(), ids.end());
  for (size_t i = 0; i < items.size(); i++)
    SetMediaItemSelected(items[i],
                                   want.count(static_cast<int>(i)) != 0);
}

int ReaperProject::RippleMode() {
  if (GetToggleCommandState(kRipplePerTrack)) return 1;
  if (GetToggleCommandState(kRippleAll)) return 2;
  return 0;
}

LedState ReaperProject::ReadLedState() {
  LedState s;
  s.snap = GetToggleCommandState(1157) == 1;
  s.ripple = RippleMode();
  s.trim_behind = GetToggleCommandState(41117) == 1;
  // active_take: the 1-based active take shared by ALL selected items,
  // else 0 (nothing selected, mixed, no active take, or take > 9 — the
  // main panel has only CAM1..9 bits).
  int shared = 0;
  for (MediaItem *item : EnumerateItems()) {
    if (!IsMediaItemSelected(item)) continue;
    const int t =
        static_cast<int>(GetMediaItemInfo_Value(item, "I_CURTAKE")) + 1;  // 1-based
    if (t < 1 || t > 9) { s.active_take = 0; return s; }
    if (shared == 0)
      shared = t;
    else if (shared != t) { s.active_take = 0; return s; }
  }
  s.active_take = shared;
  s.isolate = isolate_active_;
  s.solo = solo_active_;
  return s;
}

void ReaperProject::Undo_Begin() {
  Undo_BeginBlock2(nullptr);
}

void ReaperProject::Undo_End(const char *label) {
  Undo_EndBlock2(nullptr, label, -1);
}

int ReaperProject::ItemTakeCount(int id) {
  MediaItem *item = ItemById(id);
  return item ? CountTakes(item) : 0;
}

int ReaperProject::ItemActiveTake(int id) {
  MediaItem *item = ItemById(id);
  if (!item) return -1;
  return static_cast<int>(GetMediaItemInfo_Value(item, "I_CURTAKE"));
}

void ReaperProject::SetItemActiveTake(int id, int take) {
  MediaItem *item = ItemById(id);
  if (!item) return;
  SetMediaItemInfo_Value(item, "I_CURTAKE", static_cast<double>(take));
  UpdateItemInProject(item);
}

double ReaperProject::ItemVolume(int id) {
  MediaItem *item = ItemById(id);
  return item ? GetMediaItemInfo_Value(item, "D_VOL") : 0.0;
}

double ReaperProject::Fps() {
  bool drop = false;
  const double fps = TimeMap_curFrameRate(nullptr, &drop);
  return fps > 0.0 ? fps : 30.0;
}

ItemGeom ReaperProject::GetGeom(int id) {
  ItemGeom g{0.0, 0.0, 0.0, 1.0, 0.0};
  MediaItem *item = ItemById(id);
  if (!item) return g;
  g.pos = GetMediaItemInfo_Value(item, "D_POSITION");
  g.len = GetMediaItemInfo_Value(item, "D_LENGTH");
  g.vol = GetMediaItemInfo_Value(item, "D_VOL");
  MediaItem_Take *take = GetActiveTake(item);
  if (take) {
    g.startoffs = GetMediaItemTakeInfo_Value(take, "D_STARTOFFS");
    g.playrate = GetMediaItemTakeInfo_Value(take, "D_PLAYRATE");
  }
  return g;
}

void ReaperProject::SetGeom(int id, const ItemGeom &g) {
  MediaItem *item = ItemById(id);
  if (!item) return;
  SetMediaItemInfo_Value(item, "D_POSITION", g.pos);
  SetMediaItemInfo_Value(item, "D_LENGTH", g.len);
  SetMediaItemInfo_Value(item, "D_VOL", g.vol);
  MediaItem_Take *take = GetActiveTake(item);
  if (take) {
    SetMediaItemTakeInfo_Value(take, "D_STARTOFFS", g.startoffs);
    SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", g.playrate);
  }
  UpdateItemInProject(item);
}

// PLACE ON TOP (M6 design §3): one new track above each source track;
// every requested item is copied onto its source track's new track at the
// same position. The copies are the only new things — the source items,
// their selection, and the track selection are left alone.
void ReaperProject::DuplicateItemsAbove(const std::vector<int> &ids) {
  const std::vector<MediaItem *> items = EnumerateItems();  // id == index
  std::vector<int> source_tracks;  // unique tracks of the valid ids
  for (int id : ids) {
    if (id < 0 || id >= static_cast<int>(items.size())) continue;
    const int t = TrackIndex(GetMediaItem_Track(items[id]));
    if (t < 0) continue;
    if (std::find(source_tracks.begin(), source_tracks.end(), t) ==
        source_tracks.end())
      source_tracks.push_back(t);
  }
  // Descending: an insert at t shifts only tracks ABOVE t, so the smaller
  // indices still to process stay valid.
  for (int i = static_cast<int>(source_tracks.size()) - 1; i >= 0; --i) {
    const int t = source_tracks[i];
    MediaTrack *src = GetTrack(nullptr, t);
    if (!src) continue;
    InsertTrackInProject(nullptr, t, 0);  // new track above src, at index t
    MediaTrack *dst = GetTrack(nullptr, t);
    if (!dst) continue;
    // ids are in Items() order (track, then pos) → copies land in pos order.
    for (int id : ids) {
      if (id < 0 || id >= static_cast<int>(items.size())) continue;
      MediaItem *si = items[id];
      if (GetMediaItem_Track(si) != src) continue;
      MediaItem *ni = AddMediaItemToTrack(dst);
      if (!ni) continue;
      char chunk[8192];
      chunk[sizeof chunk - 1] = 0;  // SetItemStateChunk wants a C string
      if (GetItemStateChunk(si, chunk, static_cast<int>(sizeof chunk), false))
        SetItemStateChunk(ni, chunk, false);
      // Same position (the state chunk may carry take/item geometry; keep
      // the item's edit position explicit per design §3).
      SetMediaItemInfo_Value(ni, "D_POSITION",
                             GetMediaItemInfo_Value(si, "D_POSITION"));
      UpdateItemInProject(ni);
    }
  }
}

// View buttons (M6 design §3). The saved previous flags live in plugin
// memory only (members of this object) — the first call saves them, the
// second restores exactly those; no REAPER undo block is involved.
void ReaperProject::ToggleIsolateTracks(const std::vector<int> &item_ids) {
  if (isolate_active_) {  // second call: restore exactly the saved visibility
    for (size_t i = 0; i < saved_isolate_.size(); ++i) {
      MediaTrack *tr = GetTrack(nullptr, i);  // track deleted meanwhile: skip
      if (!tr) continue;
      SetMediaTrackInfo_Value(tr, "B_SHOWINTCP",
                              saved_isolate_[i].tcp ? 1.0 : 0.0);
      SetMediaTrackInfo_Value(tr, "B_SHOWINMIXER",
                              saved_isolate_[i].mixer ? 1.0 : 0.0);
    }
    isolate_active_ = false;
    saved_isolate_.clear();
    return;
  }
  const std::vector<MediaItem *> items = EnumerateItems();  // id == index
  const std::vector<int> targets = TracksOfItems(items, item_ids);
  if (targets.empty()) return;  // no valid target: no state change
  const int n = CountTracks(nullptr);
  saved_isolate_.resize(n);
  for (int i = 0; i < n; ++i) {
    MediaTrack *tr = GetTrack(nullptr, i);
    if (!tr) continue;
    saved_isolate_[i].tcp =
        GetMediaTrackInfo_Value(tr, "B_SHOWINTCP") != 0.0;
    saved_isolate_[i].mixer =
        GetMediaTrackInfo_Value(tr, "B_SHOWINMIXER") != 0.0;
  }
  for (int i = 0; i < n; ++i) {
    MediaTrack *tr = GetTrack(nullptr, i);
    if (!tr) continue;
    const bool show =
        std::find(targets.begin(), targets.end(), i) != targets.end();
    SetMediaTrackInfo_Value(tr, "B_SHOWINTCP", show ? 1.0 : 0.0);
    SetMediaTrackInfo_Value(tr, "B_SHOWINMIXER", show ? 1.0 : 0.0);
  }
  isolate_active_ = true;
}

void ReaperProject::ToggleSoloTracks(const std::vector<int> &item_ids) {
  if (solo_active_) {  // second call: restore exactly the saved solo flags
    for (const auto &p : saved_solo_) {
      MediaTrack *tr = GetTrack(nullptr, p.first);  // deleted meanwhile: skip
      if (!tr) continue;
      SetMediaTrackInfo_Value(tr, "I_SOLO", p.second ? 1.0 : 0.0);
    }
    solo_active_ = false;
    saved_solo_.clear();
    return;
  }
  const std::vector<MediaItem *> items = EnumerateItems();  // id == index
  const std::vector<int> targets = TracksOfItems(items, item_ids);
  if (targets.empty()) return;  // no valid target: no state change
  saved_solo_.clear();
  for (const int t : targets) {
    MediaTrack *tr = GetTrack(nullptr, t);
    if (!tr) continue;
    saved_solo_.emplace_back(t, GetMediaTrackInfo_Value(tr, "I_SOLO") != 0.0);
    SetMediaTrackInfo_Value(tr, "I_SOLO", 1.0);
  }
  solo_active_ = true;
}
