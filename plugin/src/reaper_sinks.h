// REAPER-backed playhead/action sinks (moved out of main.cpp).
//
// Not in PLUGIN_PURE_SOURCES: the method bodies call the REAPER host API
// (reaper_sinks.cpp includes reaper_api.h without REAPERAPI_IMPLEMENT —
// main.cpp owns the globals), so the .cpp is linked into the plugin target
// only (plugin/CMakeLists.txt), next to reaper_project.cpp. main.cpp
// constructs the g_reaper_sink / g_action_sink globals and hands them to
// JogEngine / Router / VerbRouter.
#ifndef SPEED_REAPER_REAPER_SINKS_H
#define SPEED_REAPER_REAPER_SINKS_H

#include "action_sink.h"
#include "jog_engine.h"  // IPlayheadSink

class ReaperPlayheadSink : public IPlayheadSink {
 public:
  // Jog = move the edit cursor by dt seconds. Measured in the headless
  // oracle: SetEditCurPos moves the cursor
  // exactly and linearly, while CSurf_ScrubAmt does not move it when the
  // transport is stopped (docs/notes/oracle-limits.md). While playing,
  // seek the playhead instead so playback follows the wheel.
  void ScrubBySeconds(double dt) override;
  double PlayPosition() const override;
};

// IZoomSource over the REAPER API: the live arrange zoom, timeline
// pixels per second (46 §4.1 seam; read every tick, so zoom + window
// resizes are tracked live).
class ReaperZoomSource : public IZoomSource {
 public:
  double PixelsPerSecond() const override;
};

// IActionSink over the REAPER API.
class ReaperActionSink : public IActionSink {
 public:
  // STOP/PLAY (40044) and friends: run a REAPER command.
  void RunCommand(int cmd) override;
  // `_Named` command (action verb): NamedCommandLookup; unresolved → trace.
  void RunCommandByName(const std::string &name) override;
  // Scroll the arrange view by dt seconds of timeline (+ = right/later).
  // Same scale as the jog cursor (Router passes (delta / kUnitsPerDetent)
  // × k_px / pxPerSec, 46 §3): the view travels the same on-screen
  // distance per wheel unit as the cursor, at any zoom level (seconds,
  // not screen lines — CSurf_OnScroll's line steps do not track zoom).
  void ScrollBySeconds(double dt) override;
  // Set the arrange view span (zoom_cursor, 46 §4.2): GetSet_ArrangeView2
  // write of L/R + UpdateArrange.
  void SetViewSpan(double L, double R) override;
  // Shuttle rate (the ShuttleRate law, shuttle.h).
  // Forward: CSurf_OnPlayRateChange, clamped to REAPER's 4.0x read-back cap
  // (the 4…8x outer half of the ShuttleRate law is flat at 4.0x in transport
  // terms; no std::min here — the SDK min/max macros are in force). Reverse: synthesized — the playhead steps back |rate|·dt where dt
  // is one engine tick (kTimerMs = 8 ms; the Router calls this once per
  // tick). The Router never passes 0; 1.0 is the JOG/SCRL tap hand-off.
  // While the transport is stopped, forward/reverse park: the wheel still
  // drives the law and playback starts on a STOP/PLAY press (reverse while
  // stopped is scrub — JOG/SCRL's job).
  void ApplyShuttleRate(double rate) override;
  // Hard stop: SHTL dead band and STOP/PLAY-while-shuttling. OnStopButton,
  // never Main_OnCommand(40045) (blocks the script/defer queue headless);
  // rate back to 1.0 for the next start.
  void HardStop() override;
  // One-line user message (AUDIO LEVEL read-out): REAPER console.
  void Message(const char *msg) override;

 private:
  double last_rate_ = 1.0; // trace dedup: the Router re-applies every tick
};

#endif  // SPEED_REAPER_REAPER_SINKS_H
