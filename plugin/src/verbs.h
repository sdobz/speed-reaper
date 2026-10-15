// verbs.h — gesture → bound verb routing (41-action-registry §4).
//
// Pure: VerbRouter parses a bindings.ini text (the embedded default when
// none is given) against the default VerbRegistry and routes each
// recognized Gesture to the verb bound to (key, gesture). Unbound
// gestures do nothing. Session pops gestures from the GestureEngine and
// hands each to OnGesture.

#ifndef SPEED_REAPER_VERBS_H
#define SPEED_REAPER_VERBS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "action_sink.h"
#include "bindings.h"
#include "gesture.h"
#include "project.h"
#include "verb_registry.h"

// The embedded default bindings.ini text (plugin/src/default_bindings.ini).
std::string_view DefaultBindingsText();

class VerbRouter {
 public:
  // Empty `bindings_text` = the embedded default.
  VerbRouter(IActionSink &sink, IProject &project,
             std::string_view bindings_text = {});
  VerbRouter(const VerbRouter &) = delete;
  VerbRouter &operator=(const VerbRouter &) = delete;

  // Run the verb bound to the gesture, if any. Press/Tap/DoubleTap → Run;
  // HoldStart/HoldRotate/HoldEnd → Begin/Step/End of the key's `turn`
  // verb. Unbound pairs are no-ops.
  void OnGesture(const Gesture &g);

  const Tuning &tuning() const { return bindings_.tuning; }
  // Keys whose tap waits for a second press (derived from `.double`).
  const std::vector<std::uint16_t> &DoubleTapKeys() const {
    return bindings_.double_tap_keys;
  }
  // Keys bound with `press` (fire on the down edge).
  const std::vector<std::uint16_t> &PressKeys() const { return press_keys_; }
  // "bindings.ini:<n>: <reason>" per skipped line.
  const std::vector<std::string> &errors() const { return bindings_.errors; }

  // The wheel mode shared by the wheel / transport verbs.
  WheelState &wheel_state() { return registry_.wheel_state; }

  // View-toggle state for the LED bar.
  bool IsolateActive() const;
  bool SoloActive() const;

 private:
  BoundVerb *Find(std::uint16_t key, BindingGesture g);

  IActionSink &sink_;
  IProject &project_;
  VerbRegistry registry_;  // before bindings_: verbs capture its WheelState
  Bindings bindings_;
  std::vector<std::uint16_t> press_keys_;
};

#endif  // SPEED_REAPER_VERBS_H
