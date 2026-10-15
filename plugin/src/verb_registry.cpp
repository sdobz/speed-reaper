// verb_registry.cpp — the named-verb table; behavior moved from the
// VerbRouter composite rows in verbs.cpp (same free functions, same
// implicit-target rule via target.h). The registry only resolves names,
// gestures, and args — it does not dispatch anything yet.
#include "verb_registry.h"

#include <cmath>     // std::log10 (AUDIO LEVEL read-out)
#include <cstdio>    // std::snprintf (AUDIO LEVEL message)
#include <cstdlib>   // std::strtol (int args)
#include <climits>   // INT_MIN / INT_MAX (int-arg range)

#include "project_ops.h"  // DuplicateItemsAbove, IsolateView, SoloView
#include "target.h"       // TargetItems, TargetTrack, WithTargetSelected
#include "trace.h"        // Trace (no-target diagnostics)

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

class ActionVerb : public Verb {
 public:
  explicit ActionVerb(int command) : command_(command) {}
  void Run(VerbContext &ctx) override { ctx.sink.RunCommand(command_); }

 private:
  int command_;
};

class TargetedVerb : public Verb {
 public:
  explicit TargetedVerb(int command) : command_(command) {}
  void Run(VerbContext &ctx) override {
    ctx.project.Undo_Begin();
    WithTargetSelected(ctx.project, [&ctx, this] { ctx.sink.RunCommand(command_); });
    ctx.project.Undo_End(("Speed Reaper: targeted " + std::to_string(command_)).c_str());
  }

 private:
  int command_;
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

}  // namespace

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
                 int id = 0;
                 std::string error;
                 if (!ParseIntArg("action", args, &id, &error))
                   return {nullptr, error};
                 return {std::make_unique<ActionVerb>(id), ""};
               }});

  reg.Register({"targeted", {BindingGesture::Press, BindingGesture::Tap,
                             BindingGesture::Double},
               "Run REAPER command <id> on the implicit targets, one undo step",
               [](std::vector<std::string> args) -> VerbBuildResult {
                 int id = 0;
                 std::string error;
                 if (!ParseIntArg("targeted", args, &id, &error))
                   return {nullptr, error};
                 return {std::make_unique<TargetedVerb>(id), ""};
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

  return reg;
}
