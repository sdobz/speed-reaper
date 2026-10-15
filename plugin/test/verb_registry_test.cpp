// verb_registry_test.cpp — the named-verb table: every name resolves,
// gestures are enforced, args are validated with a message. The verbs'
// behavior itself (moved from verbs.cpp) is spot-checked here; the
// routing-level behavior stays covered by verbs_test.cpp.
#include <cstring>  // std::strlen (error-prefix checks)
#include <string>
#include <vector>

#include "verb_registry.h"

#include "check.h"
#include "fake_project.h"

namespace {

// Records every action the verbs land (same shape as verbs_test.cpp).
class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { commands.push_back(cmd); }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override { ++hard_stops; }
  void Message(const char *msg) override { messages.push_back(msg); }

  std::vector<int> commands;
  std::vector<std::string> messages;
  int hard_stops = 0;
};

const char *kAllNames[] = {
    "action",         "targeted",    "ripple_delete",   "select_cursor_item",
    "take",           "volume_readout", "smooth_cut",   "place_on_top",
    "isolate_tracks", "solo_tracks",
};

const char *kNudgeModes[] = {"move", "trim_in", "trim_out",
                             "slip_src", "slip_dest", "volume"};

// Every name resolves and Builds under a gesture it accepts.
void test_all_names_resolve() {
  VerbRegistry reg = MakeDefaultRegistry();
  CHECK(reg.Names().size() == 12);
  for (const char *name : kAllNames) {
    std::vector<std::string> args;
    if (std::string(name) == "action" || std::string(name) == "targeted")
      args = {"40012"};
    if (std::string(name) == "take") args = {"2"};
    CHECK(reg.Find(name) != nullptr);
    VerbBuildResult r = reg.Build(name, BindingGesture::Tap, args);
    CHECK(r.error.empty());
    CHECK(r.verb != nullptr);
  }
  // The two turn verbs: both resolve under Turn (nudge under all 6 modes).
  {
    VerbBuildResult r = reg.Build("select_step", BindingGesture::Turn, {});
    CHECK(r.error.empty());
    CHECK(r.verb != nullptr);
    for (const char *mode : kNudgeModes) {
      VerbBuildResult m = reg.Build("nudge", BindingGesture::Turn, {mode});
      CHECK(m.error.empty());
      CHECK(m.verb != nullptr);
    }
  }
  // The gesture sets: action/targeted take press+tap+double;
  // ripple_delete/take take tap+double; the other six are tap only.
  for (const char *name : kAllNames) {
    const VerbSpec *spec = reg.Find(name);
    CHECK(spec != nullptr);
    if (std::string(name) == "action" || std::string(name) == "targeted") {
      CHECK(reg.Build(name, BindingGesture::Press, {"40012"}).error.empty());
      CHECK(reg.Build(name, BindingGesture::Double, {"40012"}).error.empty());
    } else if (std::string(name) == "ripple_delete" || std::string(name) == "take") {
      std::vector<std::string> dbl_args =
          std::string(name) == "take" ? std::vector<std::string>{"2"}
                                      : std::vector<std::string>{};
      CHECK(reg.Build(name, BindingGesture::Double, dbl_args).error.empty());
      CHECK(!reg.Build(name, BindingGesture::Press, std::vector<std::string>{}).error.empty());
    } else {
      CHECK(!reg.Build(name, BindingGesture::Press, std::vector<std::string>{}).error.empty());
    }
    CHECK(spec->description.size() > 0);
  }
}

// A gesture the spec does not accept is rejected with a message naming
// both the verb and the gesture.
void test_wrong_gesture_rejected() {
  VerbRegistry reg = MakeDefaultRegistry();
  for (const char *name : kAllNames) {
    VerbBuildResult r = reg.Build(name, BindingGesture::Turn,
                                  std::vector<std::string>{"1"});
    CHECK(r.verb == nullptr);
    CHECK(r.error.find(name) != std::string::npos);
    CHECK(r.error.find("turn") != std::string::npos);
  }
  // spot checks in both directions
  VerbBuildResult r1 = reg.Build("smooth_cut", BindingGesture::Press, {});
  CHECK(r1.error == "verb 'smooth_cut' does not accept gesture 'press'");
  VerbBuildResult r2 = reg.Build("take", BindingGesture::Turn, {"2"});
  CHECK(r2.error == "verb 'take' does not accept gesture 'turn'");
  VerbBuildResult r3 = reg.Build("isolate_tracks", BindingGesture::Double, {});
  CHECK(r3.error == "verb 'isolate_tracks' does not accept gesture 'double'");
  // the turn verbs reject the one-shot gestures
  CHECK(!reg.Build("select_step", BindingGesture::Tap, {}).error.empty());
  CHECK(!reg.Build("nudge", BindingGesture::Press, {"move"}).error.empty());
  // the tap verbs reject turn
  CHECK(!reg.Build("ripple_delete", BindingGesture::Turn, {}).error.empty());
}

// Bad args are rejected with a non-empty message starting with the verb
// name; the unknown-verb message names the verb.
void test_bad_args_rejected() {
  VerbRegistry reg = MakeDefaultRegistry();
  struct Case {
    const char *name;
    std::vector<std::string> args;
  };
  const Case cases[] = {
      {"take", {"0"}},        // N must be >= 1
      {"take", {"abc"}},      // not an integer
      {"take", {}},           // missing arg
      {"take", {"2", "3"}},   // too many
      {"action", {"abc"}},    // not an integer
      {"action", {}},         // missing arg
      {"targeted", {"abc"}},  // not an integer
      {"ripple_delete", {"x"}},  // no args allowed
  };
  for (const Case &c : cases) {
    VerbBuildResult r = reg.Build(c.name, BindingGesture::Tap, c.args);
    CHECK(r.verb == nullptr);
    CHECK(!r.error.empty());
    CHECK(r.error.compare(0, std::strlen(c.name), c.name) == 0);  // starts with the verb name
  }
  // The turn verbs' bad args (built under their accepted gesture, so the
  // factory — not the gesture check — rejects).
  const Case turn_cases[] = {
      {"nudge", {"foo"}},         // unknown mode
      {"nudge", {}},              // missing arg
      {"nudge", {"move", "x"}},   // too many
      {"select_step", {"1"}},     // no args allowed
  };
  for (const Case &c : turn_cases) {
    VerbBuildResult r = reg.Build(c.name, BindingGesture::Turn, c.args);
    CHECK(r.verb == nullptr);
    CHECK(!r.error.empty());
    CHECK(r.error.compare(0, std::strlen(c.name), c.name) == 0);
  }
  VerbBuildResult r = reg.Build("nope", BindingGesture::Tap, {});
  CHECK(r.verb == nullptr);
  CHECK(r.error == "unknown verb 'nope'");
}

// Behavior spot checks: the moved-verbatim logic still does what verbs.cpp
// did (deeper coverage lives in verbs_test.cpp).
void test_verb_behaviors() {
  VerbRegistry reg = MakeDefaultRegistry();

  // action: runs the command directly.
  {
    FakeProject project;
    FakeSink s;
    VerbContext c{s, project};
    auto built = reg.Build("action", BindingGesture::Tap, {"40012"});
    built.verb->Run(c);
    CHECK(s.commands.size() == 1);
    CHECK(s.commands[0] == 40012);
  }

  // targeted: one undo block, the command runs inside WithTargetSelected,
  // the original (empty) selection is restored.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 1.0, 2.0, false});
    project.cursor = 1.5;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto built = reg.Build("targeted", BindingGesture::Double, {"40511"});
    built.verb->Run(c);
    CHECK(s.commands.size() == 1);
    CHECK(s.commands[0] == 40511);
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: targeted 40511");
    CHECK(project.selected_calls.size() == 2);
    CHECK(project.selected_calls[0][0] == 0);  // target selected
    CHECK(project.selected_calls[1].empty());  // restored
  }

  // ripple_delete: time selection → 40201; no time sel + cursor target →
  // [40310, 40006, 40309] in one undo block; no target → nothing.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;
    project.last_touched = 0;
    project.time_sel = true;
    project.ripple = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("ripple_delete", BindingGesture::Tap, {}).verb->Run(c);
    CHECK((s.commands == std::vector<int>{40201}));
  }
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;
    project.last_touched = 0;
    project.ripple = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("ripple_delete", BindingGesture::Double, {}).verb->Run(c);
    CHECK((s.commands == std::vector<int>{40310, 40006, 40309}));
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: delete");
  }
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.cursor = 5.0;  // gap: no target
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("ripple_delete", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(s.commands.empty());
    CHECK(project.undo_begins == 0);
  }

  // select_cursor_item: exclusive selection; sole selection → deselect.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("select_cursor_item", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0] == std::vector<int>{0});
  }
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, true});  // sole selection
    project.cursor = 2.5;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("select_cursor_item", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(project.selected_calls.size() == 1);
    CHECK(project.selected_calls[0].empty());
  }

  // take: sets the active take on the targets, one undo block labelled
  // "Speed Reaper: take N".
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, true});
    project.take_counts = {3};
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("take", BindingGesture::Tap, {"2"}).verb->Run(c);
    CHECK(project.set_take_calls.size() == 1);
    CHECK(project.set_take_calls[0].first == 0);
    CHECK(project.set_take_calls[0].second == 1);
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: take 2");
  }

  // volume_readout: "Volume: <x.x> dB", pure read-out.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.item_volumes = {0.5};
    project.cursor = 2.5;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("volume_readout", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(s.messages.size() == 1);
    CHECK(s.messages[0] == "Volume: -6.0 dB");
    CHECK(s.commands.empty());
  }

  // smooth_cut: 40916 with a time selection, nothing without one.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.time_sel = true;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("smooth_cut", BindingGesture::Tap, {}).verb->Run(c);
    CHECK((s.commands == std::vector<int>{40916}));
  }
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("smooth_cut", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(s.commands.empty());
  }

  // place_on_top: the duplication op + one undo block.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("place_on_top", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(project.copy_calls.size() == 1);
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: place on top");
  }

  // isolate_tracks / solo_tracks: the view ops toggle on the targets'
  // tracks; a second Build+Run would restore — here: one tap activates.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});  // track 0
    project.items.push_back(ItemInfo{1, 1, 0.0, 5.0, false});  // track 1
    project.cursor = 2.5;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    reg.Build("isolate_tracks", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(project.track_views[1].show_tcp == false);  // non-target hidden
    CHECK(project.track_views[0].show_tcp == true);   // target kept

    reg.Build("solo_tracks", BindingGesture::Tap, {}).verb->Run(c);
    CHECK(project.track_views[0].solo == 1);
    CHECK(project.track_views[1].solo == 0);
    CHECK(s.commands.empty());
  }
}

// The turn verbs: the moved-verbatim hold+rotate logic (the FakeProject
// records SetSelectedItems but does not apply it, so "the state after a
// step" is simulated by copying the last recorded selection into
// items[].selected).
void test_turn_verb_behaviors() {
  VerbRegistry reg = MakeDefaultRegistry();

  auto apply_selection = [](FakeProject &p) {
    for (ItemInfo &it : p.items) it.selected = false;
    if (p.selected_calls.empty()) return;
    for (const int id : p.selected_calls.back())
      if (id >= 0 && id < static_cast<int>(p.items.size())) p.items[id].selected = true;
  };

  // select_step: one step per ±20 accumulated ticks, exclusive selection,
  // clamped at the ends; the remainder is kept across Step calls.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});   // A
    project.items.push_back(ItemInfo{1, 0, 5.0, 5.0, false});   // B
    project.items.push_back(ItemInfo{2, 0, 10.0, 5.0, false});  // C
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("select_step", BindingGesture::Turn, {}).verb;
    CHECK(v != nullptr);

    // 20 ticks: nothing selected on the track → the first item.
    v->Step(c, 20);
    CHECK(project.selected_calls.size() == 1);
    CHECK((project.selected_calls[0] == std::vector<int>{0}));
    apply_selection(project);
    // the next 20 ticks: the next item.
    v->Step(c, 20);
    CHECK(project.selected_calls.size() == 2);
    CHECK((project.selected_calls[1] == std::vector<int>{1}));
    apply_selection(project);
    // 10 + 10 ticks: the remainder is kept, so one step exactly once.
    v->Step(c, 10);
    CHECK(project.selected_calls.size() == 2);  // 10 < 20: no step yet
    v->Step(c, 10);
    CHECK(project.selected_calls.size() == 3);
    CHECK((project.selected_calls[2] == std::vector<int>{2}));
    apply_selection(project);
    // clamped at the end: another +20 stays on the last item.
    v->Step(c, 20);
    CHECK(project.selected_calls.size() == 4);
    CHECK((project.selected_calls[3] == std::vector<int>{2}));
    apply_selection(project);
    // negative: −20 → the previous item; −40 → two steps, clamped at 0.
    v->Step(c, -20);
    CHECK(project.selected_calls.size() == 5);
    CHECK((project.selected_calls[4] == std::vector<int>{1}));
    apply_selection(project);
    v->Step(c, -40);
    CHECK(project.selected_calls.size() == 7);
    CHECK((project.selected_calls[5] == std::vector<int>{0}));
    CHECK((project.selected_calls[6] == std::vector<int>{0}));
    // no target track (nothing touched/selected): a no-op.
    FakeProject p2;
    FakeSink s2;
    VerbContext c2{s2, p2};
    v->Step(c2, 40);
    CHECK(p2.selected_calls.empty());
  }

  // nudge move: one undo block per hold; one 1/Fps() step per 4 ticks
  // (3-tick remainders accumulate); the selection snapshot is restored
  // on End; stray Step/End after the hold are no-ops.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, true});  // target: selected
    project.fps = 30.0;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("nudge", BindingGesture::Turn, {"move"}).verb;
    CHECK(v != nullptr);

    v->Begin(c);
    CHECK(project.undo_begins == 1);  // the hold's single undo block
    CHECK(project.undo_ends == 0);
    v->Step(c, 3);
    CHECK(project.items[0].pos == 0.0);  // 3 ticks < one step (4)
    v->Step(c, 5);  // 8 total → 2 steps of 1/30 s
    CHECK_NEAR(project.items[0].pos, 2.0 / 30.0, 1e-12);
    CHECK(project.undo_begins == 1);  // still one block for the whole hold
    CHECK(project.undo_ends == 0);
    CHECK(s.commands.empty());  // pure project API, no REAPER commands

    v->End(c);
    CHECK(project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: move");
    CHECK(project.selected_calls.size() == 1);  // snapshot restore on release
    CHECK((project.selected_calls[0] == std::vector<int>{0}));

    // After the hold: a stray Step/End is a no-op.
    v->Step(c, 8);
    v->End(c);
    CHECK_NEAR(project.items[0].pos, 2.0 / 30.0, 1e-12);  // unchanged
    CHECK(project.undo_begins == 1);
    CHECK(project.undo_ends == 1);
  }

  // nudge trim_out: the mode's apply function runs (len += steps*1/Fps()
  // per nudge.h) under the mode's Undo_End label.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, false});
    project.cursor = 2.5;  // cursor target; nothing selected
    project.last_touched = 0;
    project.fps = 30.0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("nudge", BindingGesture::Turn, {"trim_out"}).verb;
    CHECK(v != nullptr);
    v->Begin(c);
    v->Step(c, 4);  // one step
    CHECK_NEAR(project.items[0].len, 5.0 + 1.0 / 30.0, 1e-12);
    v->End(c);
    CHECK(project.last_undo_label == "Speed Reaper: trim out");
    CHECK(project.selected_calls.size() == 1);  // empty snapshot restored
    CHECK(project.selected_calls[0].empty());
  }

  // nudge with no target (cursor in a gap, nothing selected): the whole
  // Begin/Step/End sequence does nothing — no undo block, no selection
  // churn, no geometry change.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 3.0, false});
    project.cursor = 5.0;  // in a gap
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("nudge", BindingGesture::Turn, {"slip_src"}).verb;
    CHECK(v != nullptr);
    v->Begin(c);
    v->Step(c, 8);
    v->End(c);
    CHECK(project.undo_begins == 0);
    CHECK(project.undo_ends == 0);
    CHECK(project.selected_calls.empty());
    CHECK(project.items[0].pos == 0.0);
    CHECK(project.items[0].len == 3.0);
  }
}

}  // namespace

int main() {
  test_all_names_resolve();
  test_wrong_gesture_rejected();
  test_bad_args_rejected();
  test_verb_behaviors();
  test_turn_verb_behaviors();
  return check_failures();
}
