// verb_registry.cpp — the named-verb table; behavior moved from the
// VerbRouter composite rows in verbs.cpp (same free functions, same
// implicit-target rule via target.h). The registry only resolves names,
// gestures, and args — it does not dispatch anything yet.
#include "verb_registry.h"

#include <algorithm> // std::clamp (TIMELINE select stepping)
#include <cmath>     // std::log10 (AUDIO LEVEL read-out)
#include <cstdio>    // std::snprintf (AUDIO LEVEL message)
#include <cstdlib>   // std::strtol (int args)
#include <climits>   // INT_MIN / INT_MAX (int-arg range)

#include "nudge.h"        // HoldNudge, Nudge* hold+rotate math
#include "project_ops.h"  // DuplicateItemsAbove, IsolateView, SoloView
#include "target.h"       // TargetItems, TargetTrack, WithTargetSelected
#include "trace.h"        // Trace (no-target diagnostics)
#include "wheel_mode.h"   // WheelMode

// REAPER command 40044: toggle play/stop (transport).
constexpr int kStopPlayCommand = 40044;

namespace {

void RiplDel(IActionSink &sink, IProject &project) {
  if (project.HasTimeSelection()) {
    sink.RunCommand(40201);
    return;
  }
  if (TargetItems(project).empty()) {
    Trace("target none");  // implicit-target rule 3: the verb does nothing
    return;
  }
  const bool ripple_off = project.RippleMode() == 0;
  project.Undo_Begin();
  WithTargetSelected(project, [&sink, ripple_off] {
    if (ripple_off) sink.RunCommand(40310);  // Set ripple editing per-track (a set, not a toggle)
    sink.RunCommand(40006);                  // Item: Remove items
    if (ripple_off) sink.RunCommand(40309);  // Set ripple editing off
  });
  project.Undo_End("Speed Reaper: delete");
}

void TimelineTap(IProject &project) {
  const std::vector<int> target = CursorTargetItem(project);
  if (target.empty()) {
    Trace("target none");
    return;
  }
  std::vector<int> selected;
  for (const ItemInfo &item : project.Items())
    if (item.selected) selected.push_back(item.id);
  if (selected.size() == 1 && selected[0] == target[0])
    project.SetSelectedItems({});  // sole selection → tap again = deselect all
  else
    project.SetSelectedItems(target);  // select the target exclusively
}

void CamSetTake(IProject &project, int n) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");  // implicit-target rule 3: the verb does nothing
    return;
  }
  project.Undo_Begin();
  WithTargetSelected(project, [&project, &targets, n] {
    for (const int id : targets)
      if (project.ItemTakeCount(id) >= n)  // fewer than N takes → keep theirs
        project.SetItemActiveTake(id, n - 1);
  });
  project.Undo_End(("Speed Reaper: take " + std::to_string(n)).c_str());
}

void AudioLevelTap(IActionSink &sink, IProject &project) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");
    return;
  }
  char msg[64];
  std::snprintf(msg, sizeof msg, "Volume: %.1f dB",
                20.0 * std::log10(project.ItemVolume(targets.front())));
  sink.Message(msg);
}

void PlaceOnTopTap(IProject &project) {
  const std::vector<int> targets = TargetItems(project);
  if (targets.empty()) {
    Trace("target none");  // implicit-target rule 3: the verb does nothing
    return;
  }
  project.Undo_Begin();
  WithTargetSelected(project, [&project, &targets] {
    DuplicateItemsAbove(project, targets);
  });
  project.Undo_End("Speed Reaper: place on top");
}

void SmthCutTap(IActionSink &sink, IProject &project) {
  if (project.HasTimeSelection()) {
    sink.RunCommand(40916);
    return;
  }
  Trace("no time selection");
}

// The verbs.

// The action/targeted verbs run either a numeric command id
// (RunCommand -> Main_OnCommand) or a `_named` command
// (RunCommandByName -> NamedCommandLookup in the REAPER sink).
class ActionVerb : public Verb {
 public:
  explicit ActionVerb(int command) : command_(command) {}
  explicit ActionVerb(const std::string &named) : named_(named) {}
  void Run(VerbContext &ctx) override {
    if (named_.empty()) ctx.sink.RunCommand(command_);
    else ctx.sink.RunCommandByName(named_);
  }

 private:
  int command_ = 0;
  std::string named_;
};

class TargetedVerb : public Verb {
 public:
  explicit TargetedVerb(int command) : command_(command) {}
  explicit TargetedVerb(const std::string &named) : named_(named) {}
  void Run(VerbContext &ctx) override {
    ctx.project.Undo_Begin();
    WithTargetSelected(ctx.project, [&ctx, this] {
      if (named_.empty()) ctx.sink.RunCommand(command_);
      else ctx.sink.RunCommandByName(named_);
    });
    ctx.project.Undo_End(("Speed Reaper: targeted " +
                          (named_.empty() ? std::to_string(command_) : named_))
                            .c_str());
  }

 private:
  int command_ = 0;
  std::string named_;
};

class RippleDeleteVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override { RiplDel(ctx.sink, ctx.project); }
};

class SelectCursorItemVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override { TimelineTap(ctx.project); }
};

class TakeVerb : public Verb {
 public:
  explicit TakeVerb(int n) : n_(n) {}
  void Run(VerbContext &ctx) override { CamSetTake(ctx.project, n_); }

 private:
  int n_;
};

class VolumeReadoutVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override { AudioLevelTap(ctx.sink, ctx.project); }
};

class SmoothCutVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override { SmthCutTap(ctx.sink, ctx.project); }
};

class PlaceOnTopVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override { PlaceOnTopTap(ctx.project); }
};

// Each view-toggle verb owns its saved-state op (plugin memory only).
class IsolateTracksVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override {
    const std::vector<int> targets = TargetItems(ctx.project);
    if (targets.empty()) {
      Trace("target none");  // implicit-target rule 3: the verb does nothing
      return;
    }
    isolate_view_.Toggle(ctx.project, targets);
  }
  bool IsolateActive() const override { return isolate_view_.IsActive(); }

 private:
  IsolateView isolate_view_;
};

class SoloTracksVerb : public Verb {
 public:
  void Run(VerbContext &ctx) override {
    const std::vector<int> targets = TargetItems(ctx.project);
    if (targets.empty()) {
      Trace("target none");  // implicit-target rule 3: the verb does nothing
      return;
    }
    solo_view_.Toggle(ctx.project, targets);
  }
  bool SoloActive() const override { return solo_view_.IsActive(); }

 private:
  SoloView solo_view_;
};

// One integer argument (a command id or take number). On success returns
// true; on failure *error is set and starts with `name:`.
bool ParseIntArg(const char *name, const std::vector<std::string> &args,
                 int *out, std::string *error) {
  if (args.size() != 1) {
    *error = std::string(name) + ": expected one int argument, got " +
             std::to_string(args.size());
    return false;
  }
  char *end = nullptr;
  const long v = std::strtol(args[0].c_str(), &end, 10);
  if (end == args[0].c_str() || *end != '\0') {
    *error = std::string(name) + ": argument '" + args[0] +
             "' is not an integer";
    return false;
  }
  if (v < INT_MIN || v > INT_MAX) {
    *error = std::string(name) + ": argument '" + args[0] + "' is out of range";
    return false;
  }
  *out = static_cast<int>(v);
  return true;
}

// No-argument verbs: any arg is a mistake.
bool CheckNoArgs(const char *name, const std::vector<std::string> &args,
                 std::string *error) {
  if (args.empty()) return true;
  *error = std::string(name) + ": takes no arguments";
  return false;
}

// One argument: a numeric command id or a `_named` command (the design's
// <id|_NamedCmd>). Bare non-numeric strings are rejected.
struct CommandArg {
  bool named = false;
  int id = 0;
  std::string name;
};
bool ParseCommandArg(const char *name, const std::vector<std::string> &args,
                     CommandArg *out, std::string *error) {
  if (args.size() != 1) {
    *error = std::string(name) + ": expected one command argument, got " +
             std::to_string(args.size());
    return false;
  }
  const std::string &a = args[0];
  char *end = nullptr;
  const long v = std::strtol(a.c_str(), &end, 10);
  if (end != a.c_str() && *end == '\0') {
    if (v < INT_MIN || v > INT_MAX) {
      *error = std::string(name) + ": argument '" + a + "' is out of range";
      return false;
    }
    out->named = false;
    out->id = static_cast<int>(v);
    return true;
  }
  if (!a.empty() && a[0] == '_') {
    out->named = true;
    out->name = a;
    return true;
  }
  *error = std::string(name) + ": argument '" + a +
           "' is not a command id or _named command";
  return false;
}

// --- turn verbs (moved verbatim from verbs.cpp) -----------------------

constexpr int kSelectStepTicks = 20;  // wheel ticks per TIMELINE select step

// One hold+rotate step: move the selection to the next (dir>0) / previous
// (dir<0) item on the target track, clamped at the ends; the cursor does
// not move. No selection on the track: + selects the first item, - the
// last. Exclusive selection of the single stepped-to item.
void StepSelection(IProject &project, int dir) {
  const int track = TargetTrack(project);
  if (track < 0) return;
  std::vector<int> ids;  // target-track item ids in pos order
  int sel_idx = -1;      // first (leftmost) selected item on the track, or -1
  for (const ItemInfo &item : project.Items()) {  // ordered by track then pos
    if (item.track != track) continue;
    if (item.selected && sel_idx < 0) sel_idx = static_cast<int>(ids.size());
    ids.push_back(item.id);
  }
  if (ids.empty()) return;
  const int last = static_cast<int>(ids.size()) - 1;
  if (sel_idx < 0) {  // nothing selected on the track: step from an end
    project.SetSelectedItems({ids[dir > 0 ? 0 : last]});
    return;
  }
  const int idx = std::clamp(sel_idx + dir, 0, last);
  project.SetSelectedItems({ids[idx]});
}

// TIMELINE select: the accumulator keeps the remainder between steps, so
// 10 + 10 ticks steps exactly once (the old VerbRouter::select_delta_).
class SelectStepVerb : public Verb {
 public:
  void Step(VerbContext &ctx, int delta) override {
    select_delta_ += delta;
    while (select_delta_ >= kSelectStepTicks ||
           select_delta_ <= -kSelectStepTicks) {
      const int dir = select_delta_ > 0 ? 1 : -1;
      select_delta_ -= dir * kSelectStepTicks;  // keep the remainder
      StepSelection(ctx.project, dir);
    }
  }

 private:
  int select_delta_ = 0;
};

// NUDGE modes: the old the old hold rows, key replaced by the binding-facing
// mode name; the Undo_End labels and apply functions are unchanged.
struct NudgeMode {
  const char *name;   // the binding arg: move | trim_in | trim_out | slip_src | slip_dest | volume
  const char *label;  // Undo_End label: "Speed Reaper: <verb>"
  ItemGeom (*apply)(const ItemGeom &, int, double);  // nudge.h free function
};

constexpr NudgeMode kNudgeModes[] = {
    {"move",      "Speed Reaper: move",        NudgeMove},
    {"trim_in",   "Speed Reaper: trim in",     NudgeTrimIn},
    {"trim_out",  "Speed Reaper: trim out",    NudgeTrimOut},
    {"slip_src",  "Speed Reaper: slip src",    NudgeSlipSrc},
    {"slip_dest", "Speed Reaper: slip dest",   NudgeSlipDest},
    {"volume",    "Speed Reaper: audio level", NudgeVolume},
};

// NUDGE hold+rotate: one hold = one undo block (Undo_Begin at Begin,
// Undo_End at End). The target set is resolved ONCE at Begin and the
// selection is snapshotted there, restored at End. Each step is one
// project frame (1/Fps()); 4 wheel ticks per step, the remainder
// accumulated in HoldNudge (signed, symmetric toward zero).
class NudgeVerb : public Verb {
 public:
  explicit NudgeVerb(const NudgeMode *mode) : mode_(mode) {}

  void Begin(VerbContext &ctx) override {
    const std::vector<int> targets = TargetItems(ctx.project);
    if (targets.empty()) {
      Trace("target none");  // no-target hold: do nothing (no undo block)
      return;
    }
    active_ = true;
    targets_ = targets;
    for (const ItemInfo &item : ctx.project.Items())
      if (item.selected) selection_.push_back(item.id);
    ctx.project.Undo_Begin();  // one hold = one undo block
  }

  void Step(VerbContext &ctx, int delta) override {
    if (!active_) return;  // unmapped / no-target hold: no-op
    const int steps = nudge_.Add(delta);
    if (steps == 0) return;  // below the 4-tick step threshold
    const double step = 1.0 / ctx.project.Fps();  // one project frame
    for (const int id : targets_)
      ctx.project.SetGeom(id, mode_->apply(ctx.project.GetGeom(id), steps, step));
  }

  void End(VerbContext &ctx) override {
    if (!active_) return;  // no hold active
    ctx.project.SetSelectedItems(selection_);  // restore snapshot
    ctx.project.Undo_End(mode_->label);
    active_ = false;
  }

 private:
  const NudgeMode *mode_;
  HoldNudge nudge_{4};
  bool active_ = false;
  std::vector<int> targets_;
  std::vector<int> selection_;
};

}  // namespace

bool WheelState::Set(WheelMode m) {
  if (m == mode) return false;  // re-pressing the active mode is not a change
  mode = m;
  mode_change_pending = true;
  ++changes;
  return true;
}

bool WheelState::TakeModeChange(WheelMode *m) {
  if (!mode_change_pending) return false;
  mode_change_pending = false;
  if (m != nullptr) *m = mode;
  return true;
}

// --- wheel + transport verbs (moved verbatim from Router::OnButton) ----

// The wheel-mode buttons (jog/scrl/shtl). JOG/SCRL tap while shuttling
// hands off (rate back to 1.0); an SHTL press remembers the prior mode
// when not already in Shtl. The shared WheelState carries the mode
// between presses.
class WheelVerb : public Verb {
 public:
  WheelVerb(WheelMode mode, WheelState *state) : mode_(mode), state_(state) {}
  void Run(VerbContext &ctx) override {
    if (mode_ != WheelMode::Shtl) {
      if (state_->mode == WheelMode::Shtl) ctx.sink.ApplyShuttleRate(1.0);
    } else {
      if (state_->mode != WheelMode::Shtl) state_->shuttle_prev = state_->mode;
    }
    state_->Set(mode_);
  }

 private:
  WheelMode mode_;
  WheelState *state_;
};

// transport play_stop: in SHTL, hard stop (OnStopButton in REAPER, never
// 40045) and back to the prior mode; outside SHTL, the plain toggle
// command.
class TransportPlayStopVerb : public Verb {
 public:
  explicit TransportPlayStopVerb(WheelState *state) : state_(state) {}
  void Run(VerbContext &ctx) override {
    if (state_->mode == WheelMode::Shtl) {
      ctx.sink.HardStop();
      state_->Set(state_->shuttle_prev);
    } else {
      ctx.sink.RunCommand(kStopPlayCommand);
    }
  }

 private:
  WheelState *state_;
};

const char *GestureName(BindingGesture g) {
  switch (g) {
    case BindingGesture::Press: return "press";
    case BindingGesture::Tap: return "tap";
    case BindingGesture::Double: return "double";
    case BindingGesture::Turn: return "turn";
  }
  return "press";
}

void VerbRegistry::Register(VerbSpec spec) { specs_[spec.name] = std::move(spec); }

const VerbSpec *VerbRegistry::Find(const std::string &name) const {
  const auto it = specs_.find(name);
  return it == specs_.end() ? nullptr : &it->second;
}

std::vector<std::string> VerbRegistry::Names() const {
  std::vector<std::string> names;
  for (const auto &kv : specs_) names.push_back(kv.first);
  return names;  // std::map: already sorted
}

VerbBuildResult VerbRegistry::Build(const std::string &name, BindingGesture gesture,
                                    const std::vector<std::string> &args) const {
  const auto it = specs_.find(name);
  if (it == specs_.end())
    return {nullptr, "unknown verb '" + name + "'"};
  const VerbSpec &spec = it->second;
  bool accepts = false;
  for (const BindingGesture g : spec.gestures)
    if (g == gesture) accepts = true;
  if (!accepts)
    return {nullptr, "verb '" + name + "' does not accept gesture '" +
                     GestureName(gesture) + "'"};
  return spec.factory(args);
}

VerbRegistry MakeDefaultRegistry() {
  VerbRegistry reg;

  reg.Register({"action", {BindingGesture::Press, BindingGesture::Tap,
                            BindingGesture::Double},
               "Run REAPER command <id> directly",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 CommandArg cmd;
                 std::string error;
                 if (!ParseCommandArg("action", args, &cmd, &error))
                   return {nullptr, error};
                 if (cmd.named) return {std::make_unique<ActionVerb>(cmd.name), ""};
                 return {std::make_unique<ActionVerb>(cmd.id), ""};
               }});

  reg.Register({"targeted", {BindingGesture::Press, BindingGesture::Tap,
                             BindingGesture::Double},
               "Run REAPER command <id> on the implicit targets, one undo step",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 CommandArg cmd;
                 std::string error;
                 if (!ParseCommandArg("targeted", args, &cmd, &error))
                   return {nullptr, error};
                 if (cmd.named) return {std::make_unique<TargetedVerb>(cmd.name), ""};
                 return {std::make_unique<TargetedVerb>(cmd.id), ""};
               }});

  reg.Register({"ripple_delete", {BindingGesture::Tap, BindingGesture::Double},
               "Remove the targets (or time selection) and close the gap",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("ripple_delete", args, &error)) return {nullptr, error};
                 return {std::make_unique<RippleDeleteVerb>(), ""};
               }});

  reg.Register({"select_cursor_item", {BindingGesture::Tap},
               "Select the item under the cursor exclusively (tap again to deselect)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("select_cursor_item", args, &error)) return {nullptr, error};
                 return {std::make_unique<SelectCursorItemVerb>(), ""};
               }});

  reg.Register({"take", {BindingGesture::Tap, BindingGesture::Double},
               "Set the active take <N> on the targets",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 int n = 0;
                 std::string error;
                 if (!ParseIntArg("take", args, &n, &error)) return {nullptr, error};
                 if (n < 1) return {nullptr, "take: N must be >= 1"};
                 return {std::make_unique<TakeVerb>(n), ""};
               }});

  reg.Register({"volume_readout", {BindingGesture::Tap},
               "Read out the first target item's volume in dB",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("volume_readout", args, &error)) return {nullptr, error};
                 return {std::make_unique<VolumeReadoutVerb>(), ""};
               }});

  reg.Register({"smooth_cut", {BindingGesture::Tap},
               "Smooth-cut at the time selection",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("smooth_cut", args, &error)) return {nullptr, error};
                 return {std::make_unique<SmoothCutVerb>(), ""};
               }});

  reg.Register({"place_on_top", {BindingGesture::Tap},
               "Duplicate the targets onto a new track above each source track",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("place_on_top", args, &error)) return {nullptr, error};
                 return {std::make_unique<PlaceOnTopVerb>(), ""};
               }});

  reg.Register({"isolate_tracks", {BindingGesture::Tap},
               "Hide every track except the targets' tracks (tap again to restore)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("isolate_tracks", args, &error)) return {nullptr, error};
                 return {std::make_unique<IsolateTracksVerb>(), ""};
               }});

  reg.Register({"solo_tracks", {BindingGesture::Tap},
               "Solo the targets' tracks (tap again to restore)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("solo_tracks", args, &error)) return {nullptr, error};
                 return {std::make_unique<SoloTracksVerb>(), ""};
               }});

  reg.Register({"select_step", {BindingGesture::Turn},
               "Step the selection to the next/previous item on the target track (20 ticks per step)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("select_step", args, &error)) return {nullptr, error};
                 return {std::make_unique<SelectStepVerb>(), ""};
               }});

  reg.Register({"nudge", {BindingGesture::Turn},
               "Hold+rotate nudge of the targets: <move|trim_in|trim_out|slip_src|slip_dest|volume>",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 if (args.size() != 1)
                   return {nullptr, "nudge: expected one of move|trim_in|trim_out|slip_src|slip_dest|volume, got " +
                                         std::to_string(args.size()) + " argument(s)"};
                 const NudgeMode *mode = nullptr;
                 for (const NudgeMode &m : kNudgeModes)
                   if (args[0] == m.name) mode = &m;
                 if (mode == nullptr)
                   return {nullptr, "nudge: unknown mode '" + args[0] + "' (expected one of move|trim_in|trim_out|slip_src|slip_dest|volume)"};
                 return {std::make_unique<NudgeVerb>(mode), ""};
               }});

  reg.Register({"wheel", {BindingGesture::Press},
               "Switch the wheel mode: <jog|scrl|shtl>",
               [&reg](std::vector<std::string> args) -> VerbBuildResult {
                 if (args.size() != 1)
                   return {nullptr, "wheel: expected one of jog|scrl|shtl, got " +
                                         std::to_string(args.size()) + " argument(s)"};
                 WheelMode mode = WheelMode::Jog;
                 if (args[0] == "jog") mode = WheelMode::Jog;
                 else if (args[0] == "scrl") mode = WheelMode::Scrl;
                 else if (args[0] == "shtl") mode = WheelMode::Shtl;
                 else
                   return {nullptr, "wheel: unknown mode '" + args[0] +
                                         "' (expected one of jog|scrl|shtl)"};
                 return {std::make_unique<WheelVerb>(mode, &reg.wheel_state), ""};
               }});

  reg.Register({"transport", {BindingGesture::Press},
               "Transport <play_stop> (a SHTL play_stop is a hard stop back to the prior mode)",
               [&reg](std::vector<std::string> args) -> VerbBuildResult {
                 if (args.size() != 1)
                   return {nullptr, "transport: expected 'play_stop', got " +
                                         std::to_string(args.size()) + " argument(s)"};
                 if (args[0] != "play_stop")
                   return {nullptr, "transport: unknown action '" + args[0] + "' (expected 'play_stop')"};
                 return {std::make_unique<TransportPlayStopVerb>(&reg.wheel_state), ""};
               }});

  return reg;
}
