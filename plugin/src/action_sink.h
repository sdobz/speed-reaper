// action_sink.h — where Router button/wheel actions land (design §6).
//
// REAPER implements this in main.cpp: RunCommand → Main_OnCommand(cmd, 0),
// ScrollView → CSurf_OnScroll(steps, 0) (steps: + = right/later,
// − = left/earlier). Unit tests provide a fake, so the Router stays
// REAPER-free.

#ifndef SPEED_REAPER_ACTION_SINK_H
#define SPEED_REAPER_ACTION_SINK_H

class IActionSink {
 public:
  virtual ~IActionSink() = default;
  // Run a REAPER command (e.g. 40044 = STOP/PLAY toggle).
  virtual void RunCommand(int cmd) = 0;
  // Scroll the arrange view by whole steps (+ = right/later).
  virtual void ScrollView(int steps) = 0;
};

#endif  // SPEED_REAPER_ACTION_SINK_H
