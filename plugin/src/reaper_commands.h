// reaper_commands.h — named REAPER command ids.
// The raw numbers appear only here (docs/notes/led-binding.md § Named ids).
#pragma once

namespace reaper_cmd {
constexpr int Snap = 1157;          // "Options: Toggle snapping"
constexpr int RipplePerTrack = 40310;   // "Set ripple editing per-track"
constexpr int RippleAll = 40311;        // "Set ripple editing all tracks"
constexpr int TrimBehind = 41117;       // "Options: Trim content behind media items when editing"
}  // namespace reaper_cmd
