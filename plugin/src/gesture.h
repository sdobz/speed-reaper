// gesture.h — button press/release + wheel input → gesture events.
//
// Pure: all time is passed in as `now_ms`;
// the engine owns no clock and calls no REAPER API. main.cpp feeds it
// ButtonChange edges and wheel deltas, calls Poll() every timer tick, and
// hands popped gestures to the verb router.

#ifndef SPEED_REAPER_GESTURE_H
#define SPEED_REAPER_GESTURE_H

#include <cstdint>
#include <map>
#include <vector>

enum class GestureKind { Press, Tap, DoubleTap, HoldStart, HoldRotate, HoldEnd };

// One recognized gesture. `delta` is only meaningful for HoldRotate (wheel
// ticks while a key is held); it is 0 for every other kind.
struct Gesture {
  std::uint16_t key = 0;
  GestureKind kind = GestureKind::Tap;
  std::int32_t delta = 0;
};

struct GestureConfig {
  int double_tap_ms = 300; // tap-wait window for keys in the double-tap set
  int hold_ms = 350;       // press duration before HoldStart fires
};

class GestureEngine {
 public:
  explicit GestureEngine(GestureConfig c = {}) : cfg_(c) {}

  // Keys whose tap waits for a possible second press instead of firing at
  // release (IN, OUT, ESC). A second press inside the window
  // yields exactly one DoubleTap and no Tap.
  void SetDoubleTapKeys(std::vector<std::uint16_t> keys);

  // Keys bound with `press`: fire a Press on the down edge, nothing else
  // (no tap wait, no hold/double tracking).
  void SetPressKeys(std::vector<std::uint16_t> keys);

  void OnKey(std::uint16_t key, bool down, std::int64_t now_ms);
  void OnWheel(std::int32_t delta, std::int64_t now_ms);
  void Poll(std::int64_t now_ms);  // call every timer tick
  bool Pop(Gesture *out);

  // True while any key is held down. main.cpp routes wheel deltas to
  // OnWheel (not the jog path) while this is true.
  bool HoldActive() const;

 private:
  struct KeyState {
    bool pressed = false;
    bool hold_started = false;
    bool suppress_release_tap = false;  // release of a double-tap's 2nd press
    bool waiting_second = false;        // double-tap key: waiting for press 2
    std::int64_t down_ms = 0;
    std::int64_t release_ms = 0;
  };

  bool IsDoubleTapKey(std::uint16_t key) const;
  void Emit(Gesture g) { pending_.push_back(g); }

  GestureConfig cfg_;
  std::vector<std::uint16_t> double_tap_keys_;
  std::vector<std::uint16_t> press_keys_;
  std::map<std::uint16_t, KeyState> keys_;  // sorted: deterministic emit order
  std::vector<Gesture> pending_;
};

#endif  // SPEED_REAPER_GESTURE_H
