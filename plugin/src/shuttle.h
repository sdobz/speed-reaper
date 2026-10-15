// shuttle.h — shuttle playback-rate law.
// Pure: no REAPER API, no HID. Input is the absolute jog offset in
// [−4096, 4096]; dead band |offset| < 40 → stop; ±1.0 at the dead-band
// edge; ±8.0 at |offset| ≥ 4096; smoothstep in between.
#ifndef SPEED_REAPER_PLUGIN_SHUTTLE_H
#define SPEED_REAPER_PLUGIN_SHUTTLE_H

#include <cstdint>

// Signed playback rate for an absolute shuttle offset: 0 in the dead band;
// otherwise rate = 1 + 7·s²(3−2s) with s = (|offset|−40)/(4096−40) clamped
// to [0,1], signed by the offset.
double ShuttleRate(std::int32_t offset);

#endif  // SPEED_REAPER_PLUGIN_SHUTTLE_H
