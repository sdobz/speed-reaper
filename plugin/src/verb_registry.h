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
#include <string>
#include <vector>

#include "action_sink.h"
#include "project.h"

// The physical gestures a binding can offer.
enum class BindingGesture { Press, Tap, Double, Turn };

// "press" / "tap" / "double" / "turn" — the binding-facing spelling.
const char *GestureName(BindingGesture g);

// Everything a verb call needs: where actions land and the project state.
struct VerbContext {
  IActionSink &sink;
  IProject &project;
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
};

// Result of a factory: exactly one of verb / error. verb is null iff
// error is non-empty.
struct VerbBuildResult {
  std::unique_ptr<Verb> verb;
  std::string error;
};

struct VerbSpec {
  std::string name;
  std::vector<BindingGesture> gestures;  // the bindings this verb accepts
  std::string description;
  std::function<VerbBuildResult(std::vector<std::string> args)> factory;
};

class VerbRegistry {
 public:
  void Register(VerbSpec spec);
  const VerbSpec *Find(const std::string &name) const;  // nullptr if unknown
  std::vector<std::string> Names() const;  // sorted

  // Full resolution: unknown name → error "unknown verb 'X'"; gesture not
  // in spec.gestures → error "verb 'X' does not accept gesture 'y'"; else
  // the factory runs (its error, if any, starts with the verb name).
  VerbBuildResult Build(const std::string &name, BindingGesture gesture,
                        const std::vector<std::string> &args) const;

 private:
  std::map<std::string, VerbSpec> specs_;
};

// The verbs that exist today (the VerbRouter tables in verbs.cpp),
// registered as named specs.
VerbRegistry MakeDefaultRegistry();

#endif  // SPEED_REAPER_VERB_REGISTRY_H
