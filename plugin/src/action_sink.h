// action_sink.h — where Router button/wheel actions land (design §6).
//
// REAPER implements this in main.cpp: RunCommand → Main_OnCommand(cmd, 0),
// ScrollBySeconds → GetSet_ArrangeView2 read/shift + UpdateArrange
// (dt: + = right/later, − = left/earlier). Unit tests provide a fake, so the
// Router stays REAPER-free.

#ifndef SPEED_REAPER_ACTION_SINK_H
#define SPEED_REAPER_ACTION_SINK_H

class IActionSink {
 public:
  virtual ~IActionSink() = default;
  // Run a REAPER command (e.g. 40044 = STOP/PLAY toggle).
  virtual void RunCommand(int cmd) = 0;
  // Scroll the arrange view by dt seconds of timeline (+ = right/later).
  // Seconds, not lines: the same timeline distance the jog cursor travels,
  // so the on-screen speed matches the cursor at any zoom level.
  virtual void ScrollBySeconds(double dt) = 0;
  // Apply the signed shuttle rate (M4, docs/plugin-design-m4-shuttle-impl.md,
  // rule 2). rate > 0: forward playback at that rate (the REAPER sink clamps
  // to its 4.0x cap). rate < 0: reverse, synthesized by seeking the playhead
  // back |rate|·dt per call. rate == 1.0: normal rate (JOG/SCRL tap hand-off
  // out of SHTL). The Router calls this once per engine tick with
  // ShuttleRate(offset); 0 is never passed (the dead band is a HardStop).
  virtual void ApplyShuttleRate(double rate) = 0;
  // Hard-stop the transport and restore rate 1.0 (SHTL dead band and
  // STOP/PLAY hand-off). REAPER: OnStopButton + CSurf_OnPlayRateChange(1.0)
  // — never Main_OnCommand(40045), which blocks headless (M4 task 3).
  virtual void HardStop() = 0;
};

#endif  // SPEED_REAPER_ACTION_SINK_H
