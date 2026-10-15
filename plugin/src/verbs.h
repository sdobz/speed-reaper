// verbs.h — recognized gesture → REAPER command id (design §7), plus
// composite verbs that need the project (M5 design §2).
//
// Pure: VerbRouter holds an IActionSink and an IProject (both REAPER-free
// interfaces) and maps (key, kind) pairs through the tables at the top of
// verbs.cpp; composite rows win over id rows for the same (key, kind).
// Unmapped gestures do nothing. main.cpp pops gestures from the
// GestureEngine and hands each to OnGesture.

#ifndef SPEED_REAPER_VERBS_H
#define SPEED_REAPER_VERBS_H

#include <map>
#include <vector>

#include "action_sink.h"
#include "gesture.h"
#include "nudge.h"  // HoldNudge (tick→step accumulator, M7 design §4)
#include "project.h"

class VerbRouter {
 public:
  VerbRouter(IActionSink &sink, IProject &project) : sink_(sink), project_(project) {}

  // Run the gesture's verb, if the (key, kind) pair is in a table: composite
  // rows win over id rows. Unmapped pairs are no-ops. Hold* kinds (M7,
  // design §4): the mapped hold keys (SPLIT/MOVE, TRIM IN, TRIM OUT,
  // SLIP SRC, SLIP DEST, AUDIO LEVEL) resolve their targets ONCE at
  // HoldStart (snapshotting the selection, opening the hold's single undo
  // block); each HoldRotate nudges the targets by whole steps of 1/Fps()
  // (4 wheel ticks per step, remainder accumulated); HoldEnd restores the
  // snapshot selection and closes the block. Unmapped hold keys and
  // no-target holds do nothing (no undo block opened).
  void OnGesture(const Gesture &g);

  // TIMELINE hold+rotate ticks since the last select step (M5 design §1.2;
  // one step per ±kSelectStepTicks, remainder kept). Public because the
  // composite rows in verbs.cpp are free functions.
  int select_delta_ = 0;

 private:
  // One active hold (M7, design §4): the targets resolved at HoldStart,
  // the selection snapshot to restore at HoldEnd, and the tick
  // accumulator (kHoldTicksPerStep = 4 ticks per step).
  struct HoldState {
    std::vector<int> targets;
    std::vector<int> selection;
    HoldNudge nudge{4};
  };

  IActionSink &sink_;
  IProject &project_;
  std::map<std::uint16_t, HoldState> holds_;  // active holds, by key
};

// The keys whose tap waits for a second press (design §7: IN, OUT, ESC).
// Feed this to GestureEngine::SetDoubleTapKeys.
std::vector<std::uint16_t> VerbDoubleTapKeys();

#endif  // SPEED_REAPER_VERBS_H
