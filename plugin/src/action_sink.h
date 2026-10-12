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
};

#endif  // SPEED_REAPER_ACTION_SINK_H
