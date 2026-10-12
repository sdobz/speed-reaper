// verbs.h — recognized gesture → REAPER command id (design §7).
//
// Pure: VerbRouter holds an IActionSink (REAPER-free interface) and maps
// (key, kind) pairs through the single table at the top of verbs.cpp.
// Unmapped gestures do nothing. main.cpp pops gestures from the
// GestureEngine and hands each to OnGesture.

#ifndef SPEED_REAPER_VERBS_H
#define SPEED_REAPER_VERBS_H

#include <vector>

#include "action_sink.h"
#include "gesture.h"

class VerbRouter {
 public:
  explicit VerbRouter(IActionSink &sink) : sink_(sink) {}

  // Run the gesture's REAPER command, if the (key, kind) pair is in the
  // table. Unmapped pairs (e.g. HoldRotate, HoldStart, HoldEnd) are no-ops.
  void OnGesture(const Gesture &g);

 private:
  IActionSink &sink_;
};

// The keys whose tap waits for a second press (design §7: IN, OUT, ESC).
// Feed this to GestureEngine::SetDoubleTapKeys.
std::vector<std::uint16_t> VerbDoubleTapKeys();

#endif  // SPEED_REAPER_VERBS_H
