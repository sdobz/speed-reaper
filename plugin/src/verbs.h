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
  // Live bindings table (LED poll: BindingLedMask, design § Mask).
  const Bindings &bindings() const { return bindings_; }
  // The verb registry backing the bindings (verb specs, StateReads).
  const VerbRegistry &registry() const { return registry_; }
  // Keys whose tap waits for a second press (derived from `.double`).
  const std::vector<std::uint16_t> &DoubleTapKeys() const {
    return bindings_.double_tap_keys;
  }
  // Keys bound with `.turn` (can enter the hold+turn path); a key not listed
  // is a pure tap key whose Tap still fires while the wheel is coasting.
  const std::vector<std::uint16_t> &TurnKeys() const {
    return bindings_.turn_keys;
  }
  // Keys bound with `press` (fire on the down edge).
  const std::vector<std::uint16_t> &PressKeys() const { return press_keys_; }
  // "bindings.ini:<n>: <reason>" per skipped line.
  const std::vector<std::string> &errors() const { return bindings_.errors; }

  // Re-parse `bindings_text` (empty = embedded default), replacing the
  // table. Wheel mode survives; view-toggle (isolate/solo) memory does not.
  void Reload(std::string_view bindings_text);
  // Number of bound (key, gesture) pairs.
  int BindingCount() const { return static_cast<int>(bindings_.bindings.size()); }
  // The live table, one line per binding grouped by key
  // ("TRIM_IN  tap   targeted 40511   <verb description>"), then a final
  // "Unbound: ..." line.
  std::vector<std::string> DescribeBindings() const;

  // Echo: while on, every fired binding prints `KEY.gesture → verb args`.
  void SetEcho(bool on) { echo_ = on; }
  bool Echo() const { return echo_; }

  // The wheel mode shared by the wheel / transport verbs.
  WheelState &wheel_state() { return registry_.wheel_state; }

  // View-toggle state for the LED bar.
  bool IsolateActive() const;
  bool SoloActive() const;

 private:
  BoundVerb *Find(std::uint16_t key, BindingGesture g);
  void Load(std::string_view text);
  void EchoFire(std::uint16_t key, BindingGesture g, const BoundVerb &b);

  IActionSink &sink_;
  IProject &project_;
  VerbRegistry registry_;  // before bindings_: verbs capture its WheelState
  Bindings bindings_;
  std::vector<std::uint16_t> press_keys_;
  bool echo_ = false;
};

#endif  // SPEED_REAPER_VERBS_H
