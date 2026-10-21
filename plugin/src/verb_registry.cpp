// verb_registry.cpp — the named-verb table; behavior moved from the
// VerbRouter composite rows in verbs.cpp (same free functions, same
// implicit-target rule via target.h). The registry only resolves names,
// gestures, and args — it does not dispatch anything yet.
#include "verb_registry.h"

#include <cmath>  // std::log10 (AUDIO LEVEL read-out)
#include <cstdio>    // std::snprintf (AUDIO LEVEL message)
#include <cstdlib>   // std::strtol (int args)
#include <climits>   // INT_MIN / INT_MAX (int-arg range)
#include <optional>  // std::optional (StateReadFor)

#include "nudge.h"            // HoldNudge, Nudge* hold+rotate math
#include "project_ops.h"      // DuplicateItemsAbove, IsolateView, SoloView, SharedActiveTake
#include "reaper_commands.h"  // reaper_cmd::* (LED action-read table)
#include "target.h"       // TargetItems, TargetTrack, WithTargetSelected
#include "trace.h"        // Trace (no-target diagnostics)
#include "wheel_mode.h"   // WheelMode
#include "zoom.h"         // ViewRange, ZoomAround (TIMELINE zoom)

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

// CAM N (docs/notes/cam-n-select.md): flip the selection bit of the item
// under the cursor on track N (1-based, index N-1); the rest of the
// selection is untouched; no item → no-op; no undo block (selection is not
// an undoable edit here).
void SelectTrackTap(IProject &project, int n) {
  const std::vector<ItemInfo> items = project.Items();
  const int id = CursorItemOnTrack(items, n - 1, project.CursorPos());
  if (id < 0) {
    Trace("target none");  // implicit-target rule 3: the verb does nothing
    return;
  }
  std::vector<int> selected;
  for (const ItemInfo &item : items) {
    bool sel = item.selected;
    if (item.id == id) sel = !sel;
    if (sel) selected.push_back(item.id);
  }
  project.SetSelectedItems(selected);
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

class SelectTrackVerb : public Verb {
 public:
  explicit SelectTrackVerb(int n) : n_(n) {}
  void Run(VerbContext &ctx) override { SelectTrackTap(ctx.project, n_); }

 private:
  int n_;
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

// LED state reads for `action <id>` bindings (design doc § Initial
// providers; values proven in docs/notes/m8-state-reads.md).
const std::map<int, StateRead> &ActionStateReads() {
  static const std::map<int, StateRead> reads = {
      {reaper_cmd::Snap,
       [](const LedReadContext &ctx) { return ctx.project.ToggleState(reaper_cmd::Snap); }},
      {reaper_cmd::TrimBehind,
       [](const LedReadContext &ctx) { return ctx.project.ToggleState(reaper_cmd::TrimBehind); }},
      {reaper_cmd::RipplePerTrack,
       [](const LedReadContext &ctx) { return ctx.project.RippleMode() == 1; }},
      {reaper_cmd::RippleAll,
       [](const LedReadContext &ctx) { return ctx.project.RippleMode() == 2; }},
  };
  return reads;
}

// --- turn verbs (moved verbatim from verbs.cpp) -----------------------

// One detent = 360 raw wheel units (docs/notes/m1-wheel-scale.md).
constexpr int kUnitsPerDetent = 360;

// TIMELINE hold+rotate (46 §4.2): each Step converts the raw wheel delta
// to signed detents (delta / 360) and re-anchors the arrange view around
// the edit cursor (ZoomAround), written back via SetViewSpan. The view is
// not an undoable edit: no undo block.
//
// The host quantises each written edge to the pixel grid, and one step
// moves the span by far less than a pixel: re-reading the view inside a
// Step would pin L (sub-pixel motion never crosses a grid line) and drift
// the cursor's screen fraction. So Begin captures the span W and q once
// and every Step chains ZoomAround off the kept ideal view; the read-back
// is never consulted. End releases the hold (a later Step without a fresh
// Begin re-reads the live view, as before).
class ZoomCursorVerb : public Verb {
 public:
  void Begin(VerbContext &ctx) override {
    const ViewRange v = ctx.project.GetView();
    const double w = v.R - v.L;
    if (w <= 0.0) return;  // degenerate: Step falls back to the live view
    double q = (ctx.project.CursorPos() - v.L) / w;
    if (q < 0.0) q = 0.0;
    if (q > 1.0) q = 1.0;
    W_ = w;
    q_ = q;
    active_ = true;
  }

  void Step(VerbContext &ctx, int delta) override {
    const double detents = static_cast<double>(delta) / kUnitsPerDetent;
    const double p = ctx.project.CursorPos();
    ViewRange view;
    if (active_) {
      // The kept ideal view: span W_ re-anchored so q_ sits at p.
      view.L = p - q_ * W_;
      view.R = view.L + W_;
    } else {
      view = ctx.project.GetView();
    }
    const ViewRange v =
        ZoomAround(view, p, detents, ctx.px_per_detent,
                   ctx.project.PixelsPerSecond());
    if (active_) W_ = v.R - v.L;  // ZoomAround keeps q_ (pivot p)
    ctx.sink.SetViewSpan(v.L, v.R);
  }

  void End(VerbContext &) override { active_ = false; }

 private:
  bool active_ = false;
  double W_ = 0.0;  // kept span, seconds (ideal, unquantised)
  double q_ = 0.0;  // kept cursor fraction of the span
};

// NUDGE modes. `apply` drives the per-item modes; the others are
// special-cased by `kind` in NudgeVerb.
enum class NudgeKind { Item, Roll, SelIn, SelOut };

struct NudgeMode {
  const char *name;   // the binding arg
  const char *label;  // Undo_End label: "Speed Reaper: <verb>"
  NudgeKind kind;
  ItemGeom (*apply)(const ItemGeom &, int, double);  // nudge.h; Item kind only
};

constexpr NudgeMode kNudgeModes[] = {
    {"move",      "Speed Reaper: move",        NudgeKind::Item, NudgeMove},
    {"trim_in",   "Speed Reaper: trim in",     NudgeKind::Item, NudgeTrimIn},
    {"trim_out",  "Speed Reaper: trim out",    NudgeKind::Item, NudgeTrimOut},
    {"slip_src",  "Speed Reaper: slip src",    NudgeKind::Item, NudgeSlipSrc},
    {"slip_dest", "Speed Reaper: slip dest",   NudgeKind::Item, NudgeSlipDest},
    {"volume",    "Speed Reaper: audio level", NudgeKind::Item, NudgeVolume},
    {"fade_in",   "Speed Reaper: fade in",     NudgeKind::Item, NudgeFadeIn},
    {"fade_out",  "Speed Reaper: fade out",    NudgeKind::Item, NudgeFadeOut},
    {"roll",      "Speed Reaper: roll",        NudgeKind::Roll, nullptr},
    {"sel_in",    "Speed Reaper: time selection start", NudgeKind::SelIn, nullptr},
    {"sel_out",   "Speed Reaper: time selection end",   NudgeKind::SelOut, nullptr},
};

constexpr const char *kNudgeModeList =
    "move|trim_in|trim_out|slip_src|slip_dest|volume|fade_in|fade_out|roll|sel_in|sel_out";

// The `step=` argument: frame (default) | grid | sample | <N>ms.
struct StepSpec {
  enum Kind { Frame, Grid, Sample, Ms } kind = Frame;
  double ms = 0.0;

  double Seconds(IProject &project) const {
    switch (kind) {
      case Grid: {
        const double g = project.GridSeconds();
        if (g > 0.0) return g;
        break;  // no grid known: fall back to a frame
      }
      case Sample: return project.SampleSeconds();
      case Ms: return ms / 1000.0;
      case Frame: break;
    }
    return 1.0 / project.Fps();
  }
};

bool ParseStep(const std::string &text, StepSpec *out) {
  if (text == "frame") { out->kind = StepSpec::Frame; return true; }
  if (text == "grid") { out->kind = StepSpec::Grid; return true; }
  if (text == "sample") { out->kind = StepSpec::Sample; return true; }
  if (text.size() > 2 && text.compare(text.size() - 2, 2, "ms") == 0) {
    const std::string num = text.substr(0, text.size() - 2);
    char *end = nullptr;
    const double v = std::strtod(num.c_str(), &end);
    if (end != num.c_str() && *end == '\0' && v > 0.0) {
      out->kind = StepSpec::Ms;
      out->ms = v;
      return true;
    }
  }
  return false;
}

// NUDGE hold+rotate: one hold = one undo block (Undo_Begin at Begin,
// Undo_End at End). Targets are resolved ONCE at Begin (item modes also
// snapshot the selection, restored at End); each step is `step=` seconds
// (default one project frame), `ticks_per_step` wheel ticks per step
// (the remainder accumulated in HoldNudge, signed, symmetric toward zero).
class NudgeVerb : public Verb {
 public:
  NudgeVerb(const NudgeMode *mode, StepSpec step) : mode_(mode), step_(step) {}

  void Begin(VerbContext &ctx) override {
    active_ = false;
    selection_.clear();
    targets_.clear();
    nudge_ = HoldNudge(ctx.ticks_per_step > 0 ? ctx.ticks_per_step : 4);
    switch (mode_->kind) {
      case NudgeKind::Item: {
        targets_ = TargetItems(ctx.project);
        if (targets_.empty()) { Trace("target none"); return; }
        for (const ItemInfo &item : ctx.project.Items())
          if (item.selected) selection_.push_back(item.id);
        break;
      }
      case NudgeKind::Roll: {
        // The edit point between the cursor item and its right neighbour
        // on the same track.
        const std::vector<int> cur = CursorTargetItem(ctx.project);
        if (cur.empty()) { Trace("target none"); return; }
        const std::vector<ItemInfo> items = ctx.project.Items();
        int right = -1;
        for (size_t i = 0; i + 1 < items.size(); ++i)
          if (items[i].id == cur[0] && items[i + 1].track == items[i].track)
            right = items[i + 1].id;
        if (right < 0) { Trace("target none"); return; }
        targets_ = {cur[0], right};
        break;
      }
      case NudgeKind::SelIn:
      case NudgeKind::SelOut: {
        double s = 0.0, e = 0.0;
        if (!ctx.project.TimeSelection(&s, &e)) return;  // none: no-op
        break;
      }
    }
    active_ = true;
    ctx.project.Undo_Begin();  // one hold = one undo block
  }

  void Step(VerbContext &ctx, int delta) override {
    if (!active_) return;  // no-target hold: no-op
    const int steps = nudge_.Add(delta);
    if (steps == 0) return;  // below the tick threshold
    const double step = step_.Seconds(ctx.project);
    switch (mode_->kind) {
      case NudgeKind::Item:
        for (const int id : targets_)
          ctx.project.SetGeom(id, mode_->apply(ctx.project.GetGeom(id), steps, step));
        break;
      case NudgeKind::Roll: {
        ItemGeom a = ctx.project.GetGeom(targets_[0]);
        ItemGeom b = ctx.project.GetGeom(targets_[1]);
        NudgeRoll(&a, &b, steps, step);
        ctx.project.SetGeom(targets_[0], a);
        ctx.project.SetGeom(targets_[1], b);
        break;
      }
      case NudgeKind::SelIn:
      case NudgeKind::SelOut: {
        double s = 0.0, e = 0.0;
        if (!ctx.project.TimeSelection(&s, &e)) {  // cleared mid-hold
          // keep the edge values the hold last wrote
          s = sel_start_; e = sel_end_;
        }
        if (mode_->kind == NudgeKind::SelIn) s = NudgeSelIn(s, e, steps, step);
        else e = NudgeSelOut(s, e, steps, step);
        sel_start_ = s;
        sel_end_ = e;
        ctx.project.SetTimeSelection(s, e);
        break;
      }
    }
  }

  void End(VerbContext &ctx) override {
    if (!active_) return;  // no hold active
    if (mode_->kind == NudgeKind::Item)
      ctx.project.SetSelectedItems(selection_);  // restore snapshot
    ctx.project.Undo_End(mode_->label);
    active_ = false;
  }

 private:
  const NudgeMode *mode_;
  StepSpec step_;
  HoldNudge nudge_{4};
  bool active_ = false;
  std::vector<int> targets_;
  std::vector<int> selection_;
  double sel_start_ = 0.0, sel_end_ = 0.0;
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

std::optional<StateRead> VerbRegistry::StateReadFor(const std::string &verb,
                                                    const std::vector<std::string> &args) const {
  const VerbSpec *spec = Find(verb);
  if (spec == nullptr || !spec->state_read) return std::nullopt;
  return spec->state_read(args);
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
               },
               [](const std::vector<std::string> &args) -> std::optional<StateRead> {
                 CommandArg cmd;
                 std::string error;
                 if (!ParseCommandArg("action", args, &cmd, &error))
                   return std::nullopt;
                 if (cmd.named) return std::nullopt;  // _named: no known state
                 const auto &reads = ActionStateReads();
                 const auto it = reads.find(cmd.id);
                 if (it == reads.end()) return std::nullopt;
                 return it->second;
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
               }, nullptr});

  reg.Register({"ripple_delete", {BindingGesture::Tap, BindingGesture::Double},
               "Remove the targets (or time selection) and close the gap",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("ripple_delete", args, &error)) return {nullptr, error};
                 return {std::make_unique<RippleDeleteVerb>(), ""};
               }, nullptr});

  reg.Register({"select_cursor_item", {BindingGesture::Tap},
               "Select the item under the cursor exclusively (tap again to deselect)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("select_cursor_item", args, &error)) return {nullptr, error};
                 return {std::make_unique<SelectCursorItemVerb>(), ""};
               }, nullptr});

  reg.Register({"take", {BindingGesture::Tap, BindingGesture::Double},
               "Set the active take <N> on the targets",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 int n = 0;
                 std::string error;
                 if (!ParseIntArg("take", args, &n, &error)) return {nullptr, error};
                 if (n < 1) return {nullptr, "take: N must be >= 1"};
                 return {std::make_unique<TakeVerb>(n), ""};
               },
               [](const std::vector<std::string> &args) -> std::optional<StateRead> {
                 int n = 0;
                 std::string error;
                 if (!ParseIntArg("take", args, &n, &error)) return std::nullopt;
                 return StateRead{[n](const LedReadContext &ctx) {
                                    return SharedActiveTake(ctx.project) == n;
                                  }};
               }});

  reg.Register({"select_track", {BindingGesture::Tap},
               "Toggle selection of the item under the cursor on track <N> (1..9)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 int n = 0;
                 std::string error;
                 if (!ParseIntArg("select_track", args, &n, &error))
                   return {nullptr, error};
                 if (n < 1 || n > 9) return {nullptr, "select_track: N must be 1..9"};
                 return {std::make_unique<SelectTrackVerb>(n), ""};
               },
               [](const std::vector<std::string> &args) -> std::optional<StateRead> {
                 int n = 0;
                 std::string error;
                 if (!ParseIntArg("select_track", args, &n, &error))
                   return std::nullopt;
                 if (n < 1 || n > 9) return std::nullopt;
                 return StateRead{[n](const LedReadContext &ctx) {
                                    const int id = CursorItemOnTrack(
                                        ctx.project.Items(), n - 1,
                                        ctx.project.CursorPos());
                                    if (id < 0) return false;
                                    for (const ItemInfo &item : ctx.project.Items())
                                      if (item.id == id) return item.selected;
                                    return false;
                                  }};
               }});

  reg.Register({"volume_readout", {BindingGesture::Tap},
               "Read out the first target item's volume in dB",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("volume_readout", args, &error)) return {nullptr, error};
                 return {std::make_unique<VolumeReadoutVerb>(), ""};
               }, nullptr});

  reg.Register({"smooth_cut", {BindingGesture::Tap},
               "Smooth-cut at the time selection",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("smooth_cut", args, &error)) return {nullptr, error};
                 return {std::make_unique<SmoothCutVerb>(), ""};
               }, nullptr});

  reg.Register({"place_on_top", {BindingGesture::Tap},
               "Duplicate the targets onto a new track above each source track",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("place_on_top", args, &error)) return {nullptr, error};
                 return {std::make_unique<PlaceOnTopVerb>(), ""};
               }, nullptr});

  reg.Register({"isolate_tracks", {BindingGesture::Tap},
               "Hide every track except the targets' tracks (tap again to restore)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("isolate_tracks", args, &error)) return {nullptr, error};
                 return {std::make_unique<IsolateTracksVerb>(), ""};
               },
               [](const std::vector<std::string> &) -> std::optional<StateRead> {
                 return StateRead{[](const LedReadContext &ctx) {
                                    return ctx.isolate_active;
                                  }};
               }});

  reg.Register({"solo_tracks", {BindingGesture::Tap},
               "Solo the targets' tracks (tap again to restore)",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("solo_tracks", args, &error)) return {nullptr, error};
                 return {std::make_unique<SoloTracksVerb>(), ""};
               },
               [](const std::vector<std::string> &) -> std::optional<StateRead> {
                 return StateRead{[](const LedReadContext &ctx) {
                                    return ctx.solo_active;
                                  }};
               }});

  reg.Register({"zoom_cursor", {BindingGesture::Turn},
               "Zoom the arrange view in (CW) / out (CCW) around the edit cursor",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 std::string error;
                 if (!CheckNoArgs("zoom_cursor", args, &error)) return {nullptr, error};
                 return {std::make_unique<ZoomCursorVerb>(), ""};
               }, nullptr});

  reg.Register({"nudge", {BindingGesture::Turn},
               std::string("Hold+rotate nudge of the targets: <") + kNudgeModeList +
                   "> [step=frame|grid|sample|<N>ms]",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 if (args.empty() || args.size() > 2)
                   return {nullptr, std::string("nudge: expected <") + kNudgeModeList +
                                         "> [step=...], got " +
                                         std::to_string(args.size()) + " argument(s)"};
                 const NudgeMode *mode = nullptr;
                 for (const NudgeMode &m : kNudgeModes)
                   if (args[0] == m.name) mode = &m;
                 if (mode == nullptr)
                   return {nullptr, "nudge: unknown mode '" + args[0] +
                                         "' (expected one of " + kNudgeModeList + ")"};
                 StepSpec step;
                 if (args.size() == 2) {
                   if (args[1].rfind("step=", 0) != 0 ||
                       !ParseStep(args[1].substr(5), &step))
                     return {nullptr, "nudge: bad step '" + args[1] +
                                           "' (expected step=frame|grid|sample|<N>ms)"};
                 }
                 return {std::make_unique<NudgeVerb>(mode, step), ""};
               }, nullptr});

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
               }, nullptr});

  reg.Register({"transport", {BindingGesture::Press},
               "Transport <play_stop> (a SHTL play_stop is a hard stop back to the prior mode)",
               [&reg](std::vector<std::string> args) -> VerbBuildResult {
                 if (args.size() != 1)
                   return {nullptr, "transport: expected 'play_stop', got " +
                                         std::to_string(args.size()) + " argument(s)"};
                 if (args[0] != "play_stop")
                   return {nullptr, "transport: unknown action '" + args[0] + "' (expected 'play_stop')"};
                 return {std::make_unique<TransportPlayStopVerb>(&reg.wheel_state), ""};
               }, nullptr});

  return reg;
}
