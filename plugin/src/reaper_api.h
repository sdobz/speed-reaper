// reaper_api.h — the SINGLE REAPER API list for this plugin.
//
// Exactly one TU (main.cpp) defines REAPERAPI_IMPLEMENT before including
// this file; that TU owns the REAPERAPI_* function-pointer globals and the
// REAPERAPI_LoadAPI definition. Every other TU includes this file plainly
// and gets extern declarations. Do not scatter REAPERAPI_WANT_* lines in
// .cpp files anymore.
//
// REAPERAPI_MINIMAL: REAPERAPI_LoadAPI only probes the functions below
// instead of all ~877 SDK names. Without this, the entry refuses to load
// on any host whose GetFunc lacks even one SDK name (docs/notes/
// oracle-load-diagnosis.md).
//
// RULE: every REAPER API function the plugin calls MUST get a
// REAPERAPI_WANT_<name> line below in the same change, or LoadAPI probes
// it as missing and the plugin refuses to load (and a plain include of
// this file without the line is a link error on REAPERAPI_<name>).
// tools/check_api_want (run by tools/check) fails the build if a called
// function has no line here.

#define REAPERAPI_MINIMAL
// --- REAPERAPI_WANT list: one line per function, comment = caller ---
#define REAPERAPI_WANT_GetResourcePath // main: bindings.ini location
#define REAPERAPI_WANT_NamedCommandLookup // ReaperActionSink::RunCommandByName
#define REAPERAPI_WANT_RecursiveCreateDirectory // main: bindings dir
#define REAPERAPI_WANT_PromptForAction // main: learn action picker
#define REAPERAPI_WANT_ReverseNamedCommandLookup // main: learned action -> _Named
#define REAPERAPI_WANT_GetSetProjectGrid // ReaperProject::GridSeconds
#define REAPERAPI_WANT_TimeMap_GetDividedBpmAtTime // ReaperProject::GridSeconds
#define REAPERAPI_WANT_GetSetProjectInfo // ReaperProject::SampleSeconds
#define REAPERAPI_WANT_GetPlayState // ReaperPlayheadSink::ScrubBySeconds
#define REAPERAPI_WANT_GetCursorPosition // ReaperPlayheadSink::ScrubBySeconds, ReaperProject::CursorPos
#define REAPERAPI_WANT_SetEditCurPos // ReaperPlayheadSink::ScrubBySeconds
#define REAPERAPI_WANT_GetPlayPosition // ReaperPlayheadSink::PlayPosition
#define REAPERAPI_WANT_ShowConsoleMsg // onTimer: HID worker log lines
#define REAPERAPI_WANT_Main_OnCommand // ReaperActionSink::RunCommand (Router STOP/PLAY)
#define REAPERAPI_WANT_EnumProjects // ReaperActionSink::ScrollBySeconds (current project)
#define REAPERAPI_WANT_GetSet_ArrangeView2 // ReaperActionSink::ScrollBySeconds (read/shift view)
#define REAPERAPI_WANT_UpdateArrange // ReaperActionSink::ScrollBySeconds (redraw)
#define REAPERAPI_WANT_GetToggleCommandState // ReaperProject::ToggleState/RippleMode (LED state reads: snap 1157 / trim-behind 41117, ripple 40310/40311)
#define REAPERAPI_WANT_CSurf_OnPlayRateChange // shuttle forward rate + HardStop
#define REAPERAPI_WANT_Master_GetPlayRate // shuttle read-back trace
#define REAPERAPI_WANT_OnStopButton // shuttle HardStop (40045 blocks headless)
#define REAPERAPI_WANT_GetPlayPositionEx // shuttle reverse: stepped seek
#define REAPERAPI_WANT_CountMediaItems // ReaperProject::Items/SetSelectedItems
#define REAPERAPI_WANT_GetMediaItem // ReaperProject::Items/SetSelectedItems
#define REAPERAPI_WANT_GetMediaItem_Track // ReaperProject::Items
#define REAPERAPI_WANT_GetMediaItemInfo_Value // ReaperProject::Items
#define REAPERAPI_WANT_IsMediaItemSelected // ReaperProject::Items
#define REAPERAPI_WANT_SetMediaItemSelected // ReaperProject::SetSelectedItems
#define REAPERAPI_WANT_CountTracks // ReaperProject::SelectedTracks
#define REAPERAPI_WANT_GetTrack // ReaperProject::SelectedTracks/LastTouchedTrack
#define REAPERAPI_WANT_GetMediaTrackInfo_Value // ReaperProject::SelectedTracks
#define REAPERAPI_WANT_GetLastTouchedTrack // ReaperProject::LastTouchedTrack
#define REAPERAPI_WANT_Undo_BeginBlock2 // ReaperProject::Undo_Begin
#define REAPERAPI_WANT_Undo_EndBlock2 // ReaperProject::Undo_End
#define REAPERAPI_WANT_GetSet_LoopTimeRange2 // ReaperProject::HasTimeSelection
#define REAPERAPI_WANT_CountTakes // ReaperProject::ItemTakeCount
#define REAPERAPI_WANT_SetMediaItemInfo_Value // ReaperProject::SetItemActiveTake
#define REAPERAPI_WANT_UpdateItemInProject // ReaperProject::SetItemActiveTake
#define REAPERAPI_WANT_InsertTrackInProject // ReaperProject::InsertTrackAbove
#define REAPERAPI_WANT_AddMediaItemToTrack // ReaperProject::CopyItemToTrack
#define REAPERAPI_WANT_GetItemStateChunk // ReaperProject::CopyItemToTrack
#define REAPERAPI_WANT_SetItemStateChunk // ReaperProject::CopyItemToTrack
#define REAPERAPI_WANT_SetMediaTrackInfo_Value // ReaperProject::SetTrackFlag
#define REAPERAPI_WANT_TimeMap_curFrameRate // ReaperProject::Fps
#define REAPERAPI_WANT_GetActiveTake // ReaperProject::GetGeom/SetGeom
#define REAPERAPI_WANT_GetMediaItemTakeInfo_Value // ReaperProject::GetGeom
#define REAPERAPI_WANT_SetMediaItemTakeInfo_Value // ReaperProject::SetGeom

#include "reaper_plugin_functions.h"
