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
  void ApplyShuttleRate(double rate) override { shuttle_rates.push_back(rate); }
  void HardStop() override { ++hard_stops; }
  void Message(const char *msg) override { messages.push_back(msg); }

  std::vector<int> commands;
  std::vector<std::string> messages;
  std::vector<double> shuttle_rates;
  int hard_stops = 0;
};

const char *kAllNames[] = {
    "action",         "targeted",    "ripple_delete",   "select_cursor_item",
    "take",           "select_track", "volume_readout", "smooth_cut",
    "place_on_top",   "isolate_tracks", "solo_tracks",  "wheel",
    "transport",
};

const char *kNudgeModes[] = {"move", "trim_in", "trim_out",
                             "slip_src", "slip_dest", "volume"};

// Every name resolves and Builds under a gesture it accepts.
void test_all_names_resolve() {
  VerbRegistry reg = MakeDefaultRegistry();
  CHECK(reg.Names().size() == 15);
  for (const char *name : kAllNames) {
    std::vector<std::string> args;
    if (std::string(name) == "action" || std::string(name) == "targeted")
      args = {"40012"};
    if (std::string(name) == "take") args = {"2"};
    if (std::string(name) == "select_track") args = {"1"};
    if (std::string(name) == "wheel") args = {"jog"};
    if (std::string(name) == "transport") args = {"play_stop"};
    const bool press_only =
        std::string(name) == "wheel" || std::string(name) == "transport";
    CHECK(reg.Find(name) != nullptr);
    VerbBuildResult r =
        reg.Build(name, press_only ? BindingGesture::Press : BindingGesture::Tap, args);
    CHECK(r.error.empty());
    CHECK(r.verb != nullptr);
  }
  // The wheel verb resolves under all three modes.
  for (const char *m : {"jog", "scrl", "shtl"}) {
    VerbBuildResult r = reg.Build("wheel", BindingGesture::Press, {m});
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
  // ripple_delete/take take tap+double; the rest are tap only.
  for (const char *name : kAllNames) {
    const VerbSpec *spec = reg.Find(name);
    CHECK(spec != nullptr);
    if (std::string(name) == "action" || std::string(name) == "targeted") {
      CHECK(reg.Build(name, BindingGesture::Press, {"40012"}).error.empty());
      CHECK(reg.Build(name, BindingGesture::Double, {"40012"}).error.empty());
    } else if (std::string(name) == "wheel" || std::string(name) == "transport") {
      // press only: every other gesture is rejected.
      CHECK(!reg.Build(name, BindingGesture::Tap, std::vector<std::string>{"x"}).error.empty());
      CHECK(!reg.Build(name, BindingGesture::Double, std::vector<std::string>{"x"}).error.empty());
      CHECK(!reg.Build(name, BindingGesture::Turn, std::vector<std::string>{"x"}).error.empty());
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
  // the press verbs reject the one-shot and turn gestures
  CHECK(!reg.Build("wheel", BindingGesture::Tap, {"jog"}).error.empty());
  CHECK(!reg.Build("wheel", BindingGesture::Turn, {"jog"}).error.empty());
  CHECK(!reg.Build("transport", BindingGesture::Tap, {"play_stop"}).error.empty());
  CHECK(!reg.Build("transport", BindingGesture::Turn, {"play_stop"}).error.empty());
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
  // The press verbs' bad args (built under their accepted gesture, so the
  // factory — not the gesture check — rejects).
  const Case press_cases[] = {
      {"wheel", {"foo"}},                // unknown mode
      {"wheel", {}},                     // missing arg
      {"wheel", {"jog", "x"}},           // too many
      {"transport", {"foo"}},            // unknown action
      {"transport", {}},                 // missing arg
      {"transport", {"play_stop", "x"}},  // too many
  };
  for (const Case &c : press_cases) {
    VerbBuildResult r = reg.Build(c.name, BindingGesture::Press, c.args);
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

// The press verbs: the wheel mode routing moved verbatim from
// Router::OnButton over the shared WheelState (the SHTL dead-band
// HardStop stays in Router::OnShuttleOffset and is not covered here).
void test_wheel_transport_behaviors() {
  VerbRegistry reg = MakeDefaultRegistry();
  WheelState &ws = reg.wheel_state;
  FakeProject project;
  FakeSink s;
  VerbContext c{s, project};
  auto run = [&](const char *name, const char *arg) {
    VerbBuildResult r = reg.Build(name, BindingGesture::Press, {arg});
    CHECK(r.error.empty());
    r.verb->Run(c);
  };
  WheelMode m;
  CHECK(ws.mode == WheelMode::Jog);

  // shtl press: mode Shtl, one pending change, consumed exactly once.
  run("wheel", "shtl");
  CHECK(ws.mode == WheelMode::Shtl);
  CHECK(ws.TakeModeChange(&m));
  CHECK(m == WheelMode::Shtl);
  CHECK(!ws.TakeModeChange(&m));
  // Re-press shtl while already in Shtl: not a change.
  run("wheel", "shtl");
  CHECK(!ws.TakeModeChange(&m));

  // jog press from Shtl: hand-off (rate back to 1.0), mode Jog, one change.
  run("wheel", "jog");
  CHECK(ws.mode == WheelMode::Jog);
  CHECK((s.shuttle_rates == std::vector<double>{1.0}));
  CHECK(ws.TakeModeChange(&m));
  CHECK(m == WheelMode::Jog);
  CHECK(!ws.TakeModeChange(&m));

  // play_stop outside Shtl: the plain toggle command, no hard stop.
  run("transport", "play_stop");
  CHECK((s.commands == std::vector<int>{40044}));
  CHECK(s.hard_stops == 0);

  // scrl press from Jog (no hand-off), then shtl: the prior mode is now
  // Scrl, so play_stop returns there.
  run("wheel", "scrl");
  CHECK(ws.mode == WheelMode::Scrl);
  CHECK(s.shuttle_rates.size() == 1);  // no hand-off rate outside Shtl
  run("wheel", "shtl");
  CHECK(ws.mode == WheelMode::Shtl);
  run("transport", "play_stop");
  CHECK(s.hard_stops == 1);
  CHECK(ws.mode == WheelMode::Scrl);
  CHECK(ws.TakeModeChange(&m));
  CHECK(m == WheelMode::Scrl);
  // play_stop again (now in Scrl): the command, not a stop.
  run("transport", "play_stop");
  CHECK((s.commands == std::vector<int>{40044, 40044}));
  CHECK(s.hard_stops == 1);
}

}  // namespace

// step= arg, hold_ticks_per_step, and the handle verbs (fades, roll, sel).
void test_nudge_handles() {
  VerbRegistry reg = MakeDefaultRegistry();
  // Arg validation.
  CHECK(reg.Build("nudge", BindingGesture::Turn, {"move", "step=grid"}).error.empty());
  CHECK(reg.Build("nudge", BindingGesture::Turn, {"move", "step=sample"}).error.empty());
  CHECK(reg.Build("nudge", BindingGesture::Turn, {"move", "step=10ms"}).error.empty());
  CHECK(reg.Build("nudge", BindingGesture::Turn, {"move", "step=0.5ms"}).error.empty());
  for (const char *bad : {"step=", "step=foo", "step=ms", "step=-3ms", "grid", "step=0ms"}) {
    VerbBuildResult r = reg.Build("nudge", BindingGesture::Turn, {"move", bad});
    CHECK(!r.error.empty() && r.error.rfind("nudge:", 0) == 0);
  }
  for (const char *m : {"fade_in", "fade_out", "roll", "sel_in", "sel_out"})
    CHECK(reg.Build("nudge", BindingGesture::Turn, {m}).error.empty());

  // step=10ms with 4 ticks/step; ticks_per_step from the context.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 5.0, true});
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project, 2};  // 2 ticks per step
    auto v = reg.Build("nudge", BindingGesture::Turn, {"move", "step=10ms"}).verb;
    v->Begin(c);
    v->Step(c, 4);  // 2 steps of 10 ms
    CHECK_NEAR(project.items[0].pos, 0.02, 1e-12);
    v->End(c);
  }
  // step=grid uses GridSeconds; step=sample uses SampleSeconds.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 1.0, 5.0, true});
    project.last_touched = 0;
    project.grid_seconds = 0.25;
    project.sample_seconds = 1.0 / 44100.0;
    FakeSink s;
    VerbContext c{s, project};
    auto g = reg.Build("nudge", BindingGesture::Turn, {"move", "step=grid"}).verb;
    g->Begin(c); g->Step(c, 4); g->End(c);
    CHECK_NEAR(project.items[0].pos, 1.25, 1e-12);
    auto sm = reg.Build("nudge", BindingGesture::Turn, {"move", "step=sample"}).verb;
    sm->Begin(c); sm->Step(c, 8); sm->End(c);
    CHECK_NEAR(project.items[0].pos, 1.25 + 2.0 / 44100.0, 1e-12);
    // unknown grid (<= 0) falls back to a frame
    project.grid_seconds = 0.0;
    project.items[0].pos = 0.0;
    g->Begin(c); g->Step(c, 4); g->End(c);
    CHECK_NEAR(project.items[0].pos, 1.0 / 30.0, 1e-12);
  }
  // fade_in: clamped to the item length, one undo block, label.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 1.0, true});
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("nudge", BindingGesture::Turn, {"fade_in", "step=100ms"}).verb;
    v->Begin(c);
    v->Step(c, 12);  // 3 steps
    CHECK_NEAR(project.GetGeom(0).fadein, 0.3, 1e-12);
    v->Step(c, 400);
    CHECK_NEAR(project.GetGeom(0).fadein, 1.0, 1e-12);  // <= length
    v->Step(c, -4000);
    CHECK_NEAR(project.GetGeom(0).fadein, 0.0, 1e-12);  // >= 0
    v->End(c);
    CHECK(project.undo_begins == 1 && project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: fade in");
  }
  // roll: cursor item A [0,4] + right neighbour B [4,10], cursor in A.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 4.0, false});
    project.items.push_back(ItemInfo{1, 0, 4.0, 6.0, false});
    project.cursor = 2.0;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("nudge", BindingGesture::Turn, {"roll", "step=100ms"}).verb;
    v->Begin(c);
    v->Step(c, 8);  // 2 steps
    CHECK_NEAR(project.items[0].len, 4.2, 1e-12);
    CHECK_NEAR(project.items[1].pos, 4.2, 1e-12);
    CHECK_NEAR(project.items[1].len, 5.8, 1e-12);
    v->End(c);
    CHECK(project.undo_begins == 1 && project.undo_ends == 1);
    CHECK(project.last_undo_label == "Speed Reaper: roll");
  }
  // roll with no right neighbour: target none, no undo block.
  {
    FakeProject project;
    project.items.push_back(ItemInfo{0, 0, 0.0, 4.0, false});
    project.cursor = 2.0;
    project.last_touched = 0;
    FakeSink s;
    VerbContext c{s, project};
    auto v = reg.Build("nudge", BindingGesture::Turn, {"roll"}).verb;
    v->Begin(c); v->Step(c, 40); v->End(c);
    CHECK(project.undo_begins == 0 && project.undo_ends == 0);
    CHECK_NEAR(project.items[0].len, 4.0, 1e-12);
  }
  // sel_in / sel_out move the time selection edges; none -> no-op.
  {
    FakeProject project;
    project.time_sel = true;
    project.tsel_start = 2.0;
    project.tsel_end = 5.0;
    FakeSink s;
    VerbContext c{s, project};
    auto in = reg.Build("nudge", BindingGesture::Turn, {"sel_in", "step=100ms"}).verb;
    in->Begin(c); in->Step(c, 12); in->End(c);
    CHECK_NEAR(project.tsel_start, 2.3, 1e-12);
    CHECK_NEAR(project.tsel_end, 5.0, 1e-12);
    auto out = reg.Build("nudge", BindingGesture::Turn, {"sel_out", "step=100ms"}).verb;
    out->Begin(c); out->Step(c, -8); out->End(c);
    CHECK_NEAR(project.tsel_end, 4.8, 1e-12);
    CHECK(project.undo_begins == 2 && project.undo_ends == 2);
    CHECK(project.last_undo_label == "Speed Reaper: time selection end");
    // no time selection: nothing
    FakeProject none;
    VerbContext cn{s, none};
    in->Begin(cn); in->Step(cn, 40); in->End(cn);
    CHECK(none.undo_begins == 0 && !none.time_sel);
  }
}

// LED plumbing: a spec with a provider resolves through StateReadFor; a
// spec without one and an unknown verb give nullopt (design doc § Types).
void test_state_read_for() {
  VerbRegistry reg;
  FakeProject project;
  // A test verb whose provider echoes the context's isolate toggle; it
  // declines to give a read for the arg-less binding.
  reg.Register({"led_on", {BindingGesture::Tap}, "provider: ctx.isolate_active",
                [](std::vector<std::string>) -> VerbBuildResult {
                  return {nullptr, "led_on: no verb (test spec)"};
                },
                [](const std::vector<std::string> &args) -> std::optional<StateRead> {
                  if (args.empty()) return std::nullopt;
                  return StateRead{[](const LedReadContext &ctx) {
                                     return ctx.isolate_active;
                                   }};
                }});
  // A spec without a provider: dark by construction.
  reg.Register({"no_led", {BindingGesture::Tap}, "no provider",
                [](std::vector<std::string>) -> VerbBuildResult {
                  return {nullptr, "no_led: no verb (test spec)"};
                }, nullptr});

  LedReadContext ctx{project};
  CHECK(reg.StateReadFor("ghost", {}).has_value() == false);  // unknown verb
  CHECK(reg.StateReadFor("no_led", {}).has_value() == false);  // no provider
  CHECK(reg.StateReadFor("led_on", {}).has_value() == false);  // provider says none
  std::optional<StateRead> read = reg.StateReadFor("led_on", {"1"});
  CHECK(read.has_value());
  CHECK((*read)(ctx) == false);
  ctx.isolate_active = true;
  ctx.solo_active = true;
  CHECK((*read)(ctx) == true);
}

// The seeded providers (design doc § Initial providers): action ids in
// ActionStateReads read live project state, take N reads the shared
// active take, isolate/solo read the context; everything else is dark.
void test_default_state_reads() {
  VerbRegistry reg = MakeDefaultRegistry();
  FakeProject project;
  LedReadContext ctx{project};

  // action 1157 (snap) follows ToggleState(1157).
  project.snap = false;
  std::optional<StateRead> snap = reg.StateReadFor("action", {"1157"});
  CHECK(snap.has_value());
  CHECK((*snap)(ctx) == false);
  project.snap = true;
  CHECK((*snap)(ctx) == true);

  // action 40310 / 40311 (ripple) follow RippleMode().
  std::optional<StateRead> per_track = reg.StateReadFor("action", {"40310"});
  std::optional<StateRead> all = reg.StateReadFor("action", {"40311"});
  CHECK(per_track.has_value() && all.has_value());
  project.ripple = 0;
  CHECK((*per_track)(ctx) == false && (*all)(ctx) == false);
  project.ripple = 1;
  CHECK((*per_track)(ctx) == true && (*all)(ctx) == false);
  project.ripple = 2;
  CHECK((*per_track)(ctx) == false && (*all)(ctx) == true);

  // An id not in ActionStateReads() has no read; neither does a
  // _named command.
  CHECK(reg.StateReadFor("action", {"41193"}).has_value() == false);
  CHECK(reg.StateReadFor("action", {"_x"}).has_value() == false);

  // take 4 reads SharedActiveTake (1-based): true iff it is 4.
  project.items = {ItemInfo{0, 0, 0.0, 1.0, true}};
  project.take_indices = {3};  // 0-based 3 = take number 4
  std::optional<StateRead> take4 = reg.StateReadFor("take", {"4"});
  CHECK(take4.has_value());
  CHECK((*take4)(ctx) == true);
  project.take_indices = {2};  // take number 3
  CHECK((*take4)(ctx) == false);
  std::optional<StateRead> take3 = reg.StateReadFor("take", {"3"});
  CHECK(take3.has_value() && (*take3)(ctx) == true);

  // isolate_tracks / solo_tracks read the context view toggles.
  std::optional<StateRead> isolate = reg.StateReadFor("isolate_tracks", {});
  std::optional<StateRead> solo = reg.StateReadFor("solo_tracks", {});
  CHECK(isolate.has_value() && solo.has_value());
  CHECK((*isolate)(ctx) == false && (*solo)(ctx) == false);
  ctx.isolate_active = true;
  ctx.solo_active = true;
  CHECK((*isolate)(ctx) == true && (*solo)(ctx) == true);

  // A verb without a provider stays dark.
  CHECK(reg.StateReadFor("ripple_delete", {}).has_value() == false);
}

int main() {
  test_nudge_handles();
  test_all_names_resolve();
  test_wrong_gesture_rejected();
  test_bad_args_rejected();
  test_verb_behaviors();
  test_turn_verb_behaviors();
  test_wheel_transport_behaviors();
  test_state_read_for();
  test_default_state_reads();
  return check_failures();
}
