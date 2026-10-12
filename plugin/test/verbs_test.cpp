// verbs_test.cpp — VerbRouter command-id table (design §7). Time is not
// involved: gestures come in pre-recognized, via a fake IActionSink.
#include "verbs.h"

#include "check.h"
#include "speed_editor/key_map.h"

namespace {

// Records every command the router runs.
class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { commands.push_back(cmd); }
  void ScrollBySeconds(double dt) override { (void)dt; }

  std::vector<int> commands;
};

Gesture G(std::uint16_t key, GestureKind kind, std::int32_t delta = 0) {
  return Gesture{key, kind, delta};
}

void test_mapped_gestures() {
  FakeSink sink;
  VerbRouter r(sink);

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
  VerbRouter r(sink);

  r.OnGesture(G(KEY_ROLL, GestureKind::Tap));  // not in the table
  CHECK(sink.commands.empty());

  r.OnGesture(G(KEY_SPLIT, GestureKind::HoldRotate, 3));  // hold verbs: no-op in M3
  CHECK(sink.commands.empty());

  r.OnGesture(G(KEY_IN, GestureKind::HoldStart));
  r.OnGesture(G(KEY_ESC, GestureKind::HoldEnd));
  CHECK(sink.commands.empty());
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
  test_double_tap_keys();
  return check_failures();
}
