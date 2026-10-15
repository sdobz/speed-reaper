// ReaperProject: IProject over the REAPER host API
// (docs/plugin-design-m5-m9.md §2). API signatures: docs/reaper-api-subset.md
// §2 (cursor), §8 (tracks & media items), §9 (undo).
// SDK/Swell headers: their min/max macros break <algorithm>/<set>
// (same gotcha as main.cpp — include STL before the macros land).
#include <algorithm>
#include <set>
#include "reaper_project.h"

// REAPER host APIs come from reaper_api.h — the single API list
// (REAPERAPI_MINIMAL + one WANT line per function). This TU includes it
// without REAPERAPI_IMPLEMENT (main.cpp owns the globals), so a called
// function with no WANT line is a link error.
#include "reaper_api.h"

namespace {

// Ripple commands (design §3): 40310 = "Set ripple editing per-track",
// 40311 = "Set ripple editing all tracks" (names: docs/reaper-actions.tsv).
constexpr int kRipplePerTrack = 40310;
constexpr int kRippleAll = 40311;

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

// Write primitives (40-cleanup §8) — the multi-step actions that combine
// them (duplicate, view toggles, LED snapshot) are the pure ops in
// project_ops.cpp.
int ReaperProject::TrackCount() { return CountTracks(nullptr); }

bool ReaperProject::ToggleState(int cmd) {
  return GetToggleCommandState(cmd) == 1;
}

int ReaperProject::InsertTrackAbove(int track) {
  InsertTrackInProject(nullptr, track, 0);  // new track above, at index `track`
  return GetTrack(nullptr, track) ? track : -1;
}

int ReaperProject::CopyItemToTrack(int item_id, int track) {
  MediaItem *src = ItemById(item_id);
  MediaTrack *dst = GetTrack(nullptr, track);
  if (!src || !dst) return -1;
  std::set<MediaItem *> pre;  // to identify the new item after the copy
  for (MediaItem *item : EnumerateItems()) pre.insert(item);
  MediaItem *ni = AddMediaItemToTrack(dst);
  if (!ni) return -1;
  char chunk[8192];
  chunk[sizeof chunk - 1] = 0;  // SetItemStateChunk wants a C string
  if (GetItemStateChunk(src, chunk, static_cast<int>(sizeof chunk), false))
    SetItemStateChunk(ni, chunk, false);
  // Same position (the state chunk may carry take/item geometry; keep
  // the item's edit position explicit per design §3).
  SetMediaItemInfo_Value(ni, "D_POSITION",
                         GetMediaItemInfo_Value(src, "D_POSITION"));
  UpdateItemInProject(ni);
  const std::vector<MediaItem *> after = EnumerateItems();  // id == index
  for (size_t i = 0; i < after.size(); ++i)
    if (pre.count(after[i]) == 0) return static_cast<int>(i);
  return -1;
}

bool ReaperProject::GetTrackFlag(int track, TrackFlag flag) {
  MediaTrack *tr = GetTrack(nullptr, track);
  if (!tr)  // missing track: report the flag's default (shown / not solo)
    return flag == TrackFlag::Solo ? false : true;
  switch (flag) {
    case TrackFlag::ShowTcp:
      return GetMediaTrackInfo_Value(tr, "B_SHOWINTCP") != 0.0;
    case TrackFlag::ShowMixer:
      return GetMediaTrackInfo_Value(tr, "B_SHOWINMIXER") != 0.0;
    case TrackFlag::Solo:
      return GetMediaTrackInfo_Value(tr, "I_SOLO") != 0.0;
  }
  return false;
}

void ReaperProject::SetTrackFlag(int track, TrackFlag flag, bool on) {
  MediaTrack *tr = GetTrack(nullptr, track);
  if (!tr) return;  // track deleted meanwhile: skip
  const char *key =
      flag == TrackFlag::ShowTcp    ? "B_SHOWINTCP"
      : flag == TrackFlag::ShowMixer ? "B_SHOWINMIXER"
                                     : "I_SOLO";
  SetMediaTrackInfo_Value(tr, key, on ? 1.0 : 0.0);
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


