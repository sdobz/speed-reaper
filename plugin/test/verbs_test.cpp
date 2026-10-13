// verbs_test.cpp — VerbRouter tables (design §7; M5 design §2 composites).
// Time is not involved: gestures come in pre-recognized, via a fake
// IActionSink and a FakeProject.
#include "verbs.h"

#include "check.h"
#include "fake_project.h"
#include "speed_editor/key_map.h"

namespace {

// Records every command the router runs.
class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { commands.push_back(cmd); }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override {}

  std::vector<int> commands;
};

Gesture G(std::uint16_t key, GestureKind kind, std::int32_t delta = 0) {
  return Gesture{key, kind, delta};
}

void test_mapped_gestures() {
  FakeSink sink;
  FakeProject project;
  VerbRouter r(sink, project);

  r.OnGesture(G(KEY_SPLIT, GestureKind::Tap));  // SPLIT Tap → 40012
  CHECK(sink.commands.size() == 1);
  CHECK(sink.commands[0] == 40012);

  r.OnGesture(G(KEY_ESC, GestureKind::Tap));  // ESC Tap → 40635 (remove time selection)
  CHECK(sink.commands.size() == 2);
  CHECK(sink.commands[1] == 40635);

  r.OnGesture(G(KEY_ESC, GestureKind::DoubleTap));  // ESC DoubleTap → 40029 (Undo)
  CHECK(sink.commands.size() == 3);
  CHECK(sink.commands[2] == 40029);

  r.OnGesture(G(KEY_IN, GestureKind::Tap));  // IN Tap → 40625 (set start point)
  CHECK(sink.commands.size() == 4);
  CHECK(sink.commands[3] == 40625);

  r.OnGesture(G(KEY_IN, GestureKind::DoubleTap));  // IN DoubleTap → 40635
  CHECK(sink.commands.size() == 5);
  CHECK(sink.commands[4] == 40635);
}

void test_unmapped_gestures_do_nothing() {
  FakeSink sink;
  FakeProject project;
  VerbRouter r(sink, project);

  r.OnGesture(G(KEY_ROLL, GestureKind::Tap));  // not in the table
  CHECK(sink.commands.empty());

  r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 3));  // hold verbs: no-op in M3
  CHECK(sink.commands.empty());

  r.OnGesture(G(KEY_IN, GestureKind::HoldStart));
  r.OnGesture(G(KEY_ESC, GestureKind::HoldEnd));
  CHECK(sink.commands.empty());
}

// M5 design §3: TRIM IN / OUT run their ids inside WithTargetSelected and
// one undo block labelled "Speed Reaper: trim" (composite rows win over id
// rows; the id rows for 40511/40512 no longer exist).
void test_trim_composites() {
  FakeSink sink;
  FakeProject project;
  project.items.push_back(ItemInfo{0, 0, 1.0, 2.0, false});
  project.cursor = 1.5;  // inside item 0; nothing selected
  project.last_touched = 0;
  VerbRouter r(sink, project);

  r.OnGesture(G(KEY_TRIM_IN, GestureKind::Tap));
  CHECK(sink.commands.size() == 1);
  CHECK(sink.commands[0] == 40511);
  CHECK(project.undo_begins == 1);
  CHECK(project.undo_ends == 1);
  CHECK(project.last_undo_label == "Speed Reaper: trim");
  // WithTargetSelected: select the implicit target, then restore the empty
  // original selection.
  CHECK(project.selected_calls.size() == 2);
  CHECK(project.selected_calls[0].size() == 1);
  CHECK(project.selected_calls[0][0] == 0);
  CHECK(project.selected_calls[1].empty());

  r.OnGesture(G(KEY_TRIM_OUT, GestureKind::Tap));
  CHECK(sink.commands.size() == 2);
  CHECK(sink.commands[1] == 40512);
  CHECK(project.undo_begins == 2);
  CHECK(project.undo_ends == 2);
  CHECK(project.last_undo_label == "Speed Reaper: trim");

  // Rule 1: an existing selection wins; the verb runs with it and restores
  // it unchanged.
  FakeSink sink2;
  FakeProject project2;
  project2.items.push_back(ItemInfo{0, 0, 1.0, 2.0, true});
  project2.last_touched = 0;
  VerbRouter r2(sink2, project2);
  r2.OnGesture(G(KEY_TRIM_IN, GestureKind::Tap));
  CHECK(sink2.commands.size() == 1);
  CHECK(sink2.commands[0] == 40511);
  CHECK(project2.selected_calls.size() == 1);  // snapshot restore only
  CHECK(project2.selected_calls[0].size() == 1);
  CHECK(project2.selected_calls[0][0] == 0);
}

void test_double_tap_keys() {
  auto keys = VerbDoubleTapKeys();
  CHECK(keys.size() == 3);
  CHECK(keys[0] == KEY_IN);
  CHECK(keys[1] == KEY_OUT);
  CHECK(keys[2] == KEY_ESC);
}

}  // namespace

int main() {
  test_mapped_gestures();
  test_unmapped_gestures_do_nothing();
  test_trim_composites();
  test_double_tap_keys();
  return check_failures();
}
