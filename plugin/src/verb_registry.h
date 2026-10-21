// verb_registry.h — the named-verb table the bindings resolve against.
//
// A VerbSpec declares a verb's name, the binding gestures it accepts, a
// human description, and a factory that turns argument strings into a Verb
// instance (per-verb state, e.g. a hold accumulator, lives in the verb).
// The registry answers three questions: does this name exist (Find/Names),
// does it accept this gesture, and — with valid args — give me the verb.
// Binding errors (unknown name, wrong gesture) are decided here, before
// the factory runs; factory errors (bad args) start with the verb name.
//
// Pure: no REAPER API, no HID. IActionSink/IProject are the only seams.

#ifndef SPEED_REAPER_VERB_REGISTRY_H
#define SPEED_REAPER_VERB_REGISTRY_H

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "action_sink.h"
#include "project.h"
#include "wheel_mode.h"

// The physical gestures a binding can offer.
enum class BindingGesture { Press, Tap, Double, Turn };

// "press" / "tap" / "double" / "turn" — the binding-facing spelling.
const char *GestureName(BindingGesture g);

// Everything a verb call needs: where actions land and the project state.
struct VerbContext {
  IActionSink &sink;
  IProject &project;
  int ticks_per_step = 4;  // [tuning] hold_ticks_per_step (turn verbs)
  double px_per_detent = 0.0;  // [tuning] wheel.pixels_per_detent (zoom_cursor)
};

// The wheel mode routing state (the old Router mode_ / shuttle_prev_ /
// mode_change_pending_). Owned by the VerbRegistry; the wheel and
// transport verbs capture a pointer to it so a jog press and the later
// play_stop press see the same mode. The SHTL dead-band HardStop flag is
// deliberately NOT here — it stays in Router::OnShuttleOffset.
struct WheelState {
  WheelMode mode = WheelMode::Jog;
  WheelMode shuttle_prev = WheelMode::Jog;  // mode to return to on SHTL exit
  bool mode_change_pending = false;
  unsigned changes = 0;  // bumped on every real change (Router re-arms its dead-band stop)

  // Re-pressing the active mode is not a change; otherwise sets mode
  // and arms mode_change_pending.
  bool Set(WheelMode m);
  // True exactly once per change (consumed); *m gets the new mode.
  bool TakeModeChange(WheelMode *m);
};

// One bound verb. Run handles one-shot gestures (press/tap/double);
// Begin/Step/End handle a turn (hold+rotate): Step gets the signed wheel
// delta. All no-op by default — a press-only verb overrides just Run.
class Verb {
 public:
  virtual ~Verb() = default;
  virtual void Run(VerbContext &) {}
  virtual void Begin(VerbContext &) {}
  virtual void Step(VerbContext &, int) {}
  virtual void End(VerbContext &) {}
  // View-toggle state for the LED bar (isolate_tracks / solo_tracks).
  virtual bool IsolateActive() const { return false; }
  virtual bool SoloActive() const { return false; }
};

// Result of a factory: exactly one of verb / error. verb is null iff
// error is non-empty.
struct VerbBuildResult {
  std::unique_ptr<Verb> verb;
  std::string error;
};

// The live state a key's LED reads against: the project plus the plugin's
// isolate/solo view toggles (carried by the router, not the project).
struct LedReadContext {
  IProject &project;
  bool isolate_active = false;
  bool solo_active = false;
};

// A verb's LED state read: true = its key's LED bit is lit.
using StateRead = std::function<bool(const LedReadContext &)>;

struct VerbSpec {
  std::string name;
  std::vector<BindingGesture> gestures;  // the bindings this verb accepts
  std::string description;
  std::function<VerbBuildResult(std::vector<std::string> args)> factory;
  // LED read provider (absent by default): given this specific binding's
  // args, the read for it — or nullopt (one-shot action, named
  // `action <name>`, …). Giving a verb an LED state = set this on register.
  std::function<std::optional<StateRead>(const std::vector<std::string> &args)>
      state_read;
};

class VerbRegistry {
 public:
  // Non-copyable: it owns the WheelState the wheel/transport verbs share.
  VerbRegistry() = default;
  VerbRegistry(const VerbRegistry &) = delete;
  VerbRegistry &operator=(const VerbRegistry &) = delete;
  VerbRegistry(VerbRegistry &&) = default;
  VerbRegistry &operator=(VerbRegistry &&) = default;

  // Shared by the `wheel` and `transport` verbs (the factories capture
  // a pointer to it, so it must outlive every verb they build).
  WheelState wheel_state;

  void Register(VerbSpec spec);
  const VerbSpec *Find(const std::string &name) const;  // nullptr if unknown
  std::vector<std::string> Names() const;  // sorted

  // Full resolution: unknown name → error "unknown verb 'X'"; gesture not
  // in spec.gestures → error "verb 'X' does not accept gesture 'y'"; else
  // the factory runs (its error, if any, starts with the verb name).
  VerbBuildResult Build(const std::string &name, BindingGesture gesture,
                        const std::vector<std::string> &args) const;

  // LED read for this specific binding: looks up the spec and runs its
  // state_read provider; no spec or no provider → nullopt (the provider
  // may itself return nullopt for this binding).
  std::optional<StateRead> StateReadFor(const std::string &verb,
                                        const std::vector<std::string> &args) const;

 private:
  std::map<std::string, VerbSpec> specs_;
};

// The verbs that exist today (the VerbRouter tables in verbs.cpp),
// registered as named specs.
VerbRegistry MakeDefaultRegistry();

#endif  // SPEED_REAPER_VERB_REGISTRY_H
