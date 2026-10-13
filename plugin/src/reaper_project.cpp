// ReaperProject: IProject over the REAPER host API
// (docs/plugin-design-m5-m9.md §2). API signatures: docs/reaper-api-subset.md
// §2 (cursor), §8 (tracks & media items), §9 (undo).
#include "reaper_project.h"

#include <algorithm>
#include <set>

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
#include "reaper_plugin_functions.h"

namespace {

// Ripple toggle commands (design §3): 40310 = "Ripple items (per track)",
// 40311 = "Ripple items (all tracks)".
constexpr int kRipplePerTrack = 40310;
constexpr int kRippleAll = 40311;

// GetSet_Loc is not in the vendored reaper_plugin_functions.h, so it is
// resolved manually by ReaperProject::Init (real REAPER exports it; the
// oracle host does not, and then HasTimeSelection reports false).
// "which" constants from the SDK reaper.h (not vendored):
// LOC_TSELSTART / LOC_TSELEND.
constexpr int kLocTselStart = 0x20;
constexpr int kLocTselEnd = 0x21;
double (*g_GetSet_Loc)(ReaProject *proj, int which, double *getnew,
                       bool is_set) = nullptr;

// Track-list index of a track (master excluded); -1 if null / not found.
int TrackIndex(MediaTrack *track) {
  if (!track) return -1;
  const int n = REAPERAPI_CountTracks(nullptr);
  for (int i = 0; i < n; i++)
    if (REAPERAPI_GetTrack(nullptr, i) == track) return i;
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
  const int n = REAPERAPI_CountMediaItems(nullptr);
  entries.reserve(n);
  for (int i = 0; i < n; i++) {
    MediaItem *item = REAPERAPI_GetMediaItem(nullptr, i);
    if (!item) continue;
    entries.push_back(
        {item, TrackIndex(REAPERAPI_GetMediaItem_Track(item)),
         REAPERAPI_GetMediaItemInfo_Value(item, "D_POSITION")});
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

}  // namespace

void ReaperProject::Init(void (*GetFunc)(const char *name)) {
  g_GetSet_Loc = (double (*)(ReaProject *, int, double *, bool))GetFunc(
      "GetSet_Loc");
}

std::vector<ItemInfo> ReaperProject::Items() {
  const std::vector<MediaItem *> items = EnumerateItems();
  std::vector<ItemInfo> out;
  out.reserve(items.size());
  for (size_t i = 0; i < items.size(); i++) {
    MediaItem *item = items[i];
    ItemInfo info;
    info.id = static_cast<int>(i);  // index in Items() order (design §2)
    info.track = TrackIndex(REAPERAPI_GetMediaItem_Track(item));
    info.pos = REAPERAPI_GetMediaItemInfo_Value(item, "D_POSITION");
    info.len = REAPERAPI_GetMediaItemInfo_Value(item, "D_LENGTH");
    info.selected = REAPERAPI_IsMediaItemSelected(item);
    out.push_back(info);
  }
  return out;
}

std::vector<int> ReaperProject::SelectedTracks() {
  std::vector<int> out;
  const int n = REAPERAPI_CountTracks(nullptr);
  for (int i = 0; i < n; i++) {
    MediaTrack *tr = REAPERAPI_GetTrack(nullptr, i);
    if (tr && REAPERAPI_GetMediaTrackInfo_Value(tr, "B_SELECTED"))
      out.push_back(i);
  }
  return out;
}

int ReaperProject::LastTouchedTrack() {
  return TrackIndex(REAPERAPI_GetLastTouchedTrack(nullptr));
}

double ReaperProject::CursorPos() { return REAPERAPI_GetCursorPosition(); }

bool ReaperProject::HasTimeSelection() {
  if (!g_GetSet_Loc) return false;  // oracle host: export not listed
  double t0 = 0.0, t1 = 0.0;
  g_GetSet_Loc(nullptr, kLocTselStart, &t0, false);
  g_GetSet_Loc(nullptr, kLocTselEnd, &t1, false);
  return t1 > t0;
}

void ReaperProject::SetSelectedItems(const std::vector<int> &ids) {
  const std::vector<MediaItem *> items = EnumerateItems();  // id == index
  std::set<int> want(ids.begin(), ids.end());
  for (size_t i = 0; i < items.size(); i++)
    REAPERAPI_SetMediaItemSelected(items[i],
                                   want.count(static_cast<int>(i)) != 0);
}

int ReaperProject::RippleMode() {
  if (REAPERAPI_GetToggleCommandState(kRipplePerTrack)) return 1;
  if (REAPERAPI_GetToggleCommandState(kRippleAll)) return 2;
  return 0;
}

void ReaperProject::Undo_Begin() {
  REAPERAPI_Undo_BeginBlock2(nullptr);
}

void ReaperProject::Undo_End(const char *label) {
  REAPERAPI_Undo_EndBlock2(nullptr, label, -1);
}
