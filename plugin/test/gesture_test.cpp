// gesture_test.cpp — GestureEngine rules (design §7). Time is synthetic.
#include "gesture.h"

#include "check.h"

namespace {

constexpr std::uint16_t kSplit = 0x2F;  // not in the double-tap set
constexpr std::uint16_t kIn = 0x07;     // double-tap key

void Press(GestureEngine &e, std::uint16_t key, std::int64_t t) {
  e.OnKey(key, true, t);
}
void Release(GestureEngine &e, std::uint16_t key, std::int64_t t) {
  e.OnKey(key, false, t);
}

std::vector<Gesture> PopAll(GestureEngine &e) {
  std::vector<Gesture> out;
  Gesture g;
  while (e.Pop(&g)) out.push_back(g);
  return out;
}

int CountKind(const std::vector<Gesture> &gs, GestureKind k) {
  int n = 0;
  for (const auto &g : gs)
    if (g.kind == k) ++n;
  return n;
}

void test_plain_tap_on_release() {
  GestureEngine e;
  e.SetDoubleTapKeys({kIn});
  Press(e, kSplit, 100);
  e.Poll(200);  // 100 ms held: below hold_ms, no gesture yet
  CHECK(PopAll(e).empty());
  Release(e, kSplit, 150);  // quick press → Tap emitted on release
  auto gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].key == kSplit);
  CHECK(gs[0].kind == GestureKind::Tap);
}

void test_double_tap_key_deferred_tap() {
  GestureEngine e;
  e.SetDoubleTapKeys({kIn});
  Press(e, kIn, 100);
  Release(e, kIn, 150);
  CHECK(PopAll(e).empty());  // deferred: nothing on release
  e.Poll(150 + 299);         // still inside the 300 ms window
  CHECK(PopAll(e).empty());
  e.Poll(150 + 300);         // window expired → one Tap
  auto gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].key == kIn);
  CHECK(gs[0].kind == GestureKind::Tap);
}

void test_two_taps_yield_one_double_tap() {
  GestureEngine e;
  e.SetDoubleTapKeys({kIn});
  Press(e, kIn, 100);
  Release(e, kIn, 150);
  Press(e, kIn, 400);  // second press inside the 300 ms window
  Release(e, kIn, 450);
  e.Poll(1000);
  auto gs = PopAll(e);
  CHECK(CountKind(gs, GestureKind::DoubleTap) == 1);
  CHECK(CountKind(gs, GestureKind::Tap) == 0);  // no Tap for either press
  CHECK(gs.size() == 1);
  CHECK(gs[0].key == kIn);
  // A later clean press/release of the same key still taps (no stuck state).
  Press(e, kIn, 2000);
  Release(e, kIn, 2050);
  e.Poll(2400);
  gs = PopAll(e);
  CHECK(CountKind(gs, GestureKind::Tap) == 1);
}

void test_hold_past_hold_ms() {
  GestureEngine e;
  e.SetDoubleTapKeys({kIn});
  e.SetTurnKeys({kSplit});  // kSplit is a turn key here: the timer hold applies
  Press(e, kSplit, 1000);
  e.Poll(1000 + 349);
  CHECK(PopAll(e).empty());  // not held long enough yet
  e.Poll(1000 + 350);
  auto gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].key == kSplit);
  CHECK(gs[0].kind == GestureKind::HoldStart);
  CHECK(e.HoldActive());
  e.Poll(1500);
  CHECK(PopAll(e).empty());  // HoldStart fires exactly once
  Release(e, kSplit, 2000);
  gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].kind == GestureKind::HoldEnd);
  CHECK(CountKind(gs, GestureKind::Tap) == 0);  // no Tap after a hold
  CHECK(!e.HoldActive());
}

void test_wheel_while_held() {
  GestureEngine e;
  e.SetDoubleTapKeys({kIn});
  e.SetTurnKeys({kSplit});  // kSplit is a turn key here: the wheel owns it
  Press(e, kSplit, 1000);
  // Wheel motion before hold_ms: HoldStart (not yet fired), then HoldRotate.
  e.OnWheel(5, 1100);
  auto gs = PopAll(e);
  CHECK(gs.size() == 2);
  CHECK(gs[0].key == kSplit);
  CHECK(gs[0].kind == GestureKind::HoldStart);
  CHECK(gs[1].key == kSplit);
  CHECK(gs[1].kind == GestureKind::HoldRotate);
  CHECK(gs[1].delta == 5);
  // A further delta emits only HoldRotate (no second HoldStart).
  e.OnWheel(-3, 1200);
  gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].kind == GestureKind::HoldRotate);
  CHECK(gs[0].delta == -3);
  CHECK(e.HoldActive());
  Release(e, kSplit, 1300);
  gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].kind == GestureKind::HoldEnd);
  // Wheel with no key held: no-op.
  e.OnWheel(7, 1400);
  CHECK(PopAll(e).empty());
}

void test_tap_only_key_coasting_wheel_stays_tap() {
  // 45-cam-n regression: a tap-only key (CAM) held while the wheel is
  // coasting must NOT become a hold. OnWheel is a no-op for it (the primary
  // turn key is 0), the release stays a Tap, and the dispatcher keeps the
  // delta on the jog path — so the selection is not lost to a phantom hold.
  GestureEngine e;
  e.SetTurnKeys({kSplit});            // kSplit is a turn key; kCam is not
  constexpr std::uint16_t kCam = 0x35;  // KEY_CAM3: tap-only
  Press(e, kCam, 1000);
  e.OnWheel(5, 1008);                 // coasting delta while the tap key is held
  CHECK(PopAll(e).empty());            // no HoldStart / HoldRotate for a tap key
  CHECK(!e.TurnHoldActive());          // the tap key does not own the wheel
  Release(e, kCam, 1050);
  auto gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].key == kCam);
  CHECK(gs[0].kind == GestureKind::Tap);  // tap still fires on release
}

void test_tap_only_key_slow_hold_still_taps() {
  // A tap-only key held past hold_ms (no wheel) is not a turn key, so the
  // time-based HoldStart never fires; the release is still a Tap.
  GestureEngine e;
  e.SetTurnKeys({kSplit});
  constexpr std::uint16_t kCam = 0x35;
  Press(e, kCam, 1000);
  e.Poll(1000 + 350);                 // past hold_ms
  CHECK(PopAll(e).empty());            // no HoldStart: not a turn key
  Release(e, kCam, 2000);
  auto gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].kind == GestureKind::Tap);
}

void test_turn_key_still_holds_and_rotates() {
  // A turn-capable key keeps the full hold+turn behavior (guard against
  // over-gating the new turn-key check).
  GestureEngine e;
  e.SetTurnKeys({kSplit});
  Press(e, kSplit, 1000);
  CHECK(e.TurnHoldActive());
  e.OnWheel(4, 1008);
  auto gs = PopAll(e);
  CHECK(gs.size() == 2);
  CHECK(gs[0].kind == GestureKind::HoldStart);
  CHECK(gs[1].kind == GestureKind::HoldRotate && gs[1].delta == 4);
  Release(e, kSplit, 1200);
  gs = PopAll(e);
  CHECK(gs.size() == 1);
  CHECK(gs[0].kind == GestureKind::HoldEnd);
}

void test_hold_active_between_start_and_end() {
  GestureEngine e;
  e.SetTurnKeys({kSplit});  // kSplit is a turn key here: the timer hold applies
  CHECK(!e.HoldActive());
  Press(e, kSplit, 0);
  CHECK(e.HoldActive());  // true from press...
  e.Poll(350);            // ...through HoldStart...
  auto gs = PopAll(e);
  CHECK(CountKind(gs, GestureKind::HoldStart) == 1);
  CHECK(e.HoldActive());
  Release(e, kSplit, 500);  // ...to HoldEnd.
  gs = PopAll(e);
  CHECK(CountKind(gs, GestureKind::HoldEnd) == 1);
  CHECK(!e.HoldActive());
}

}  // namespace

int main() {
  test_plain_tap_on_release();
  test_double_tap_key_deferred_tap();
  test_two_taps_yield_one_double_tap();
  test_hold_past_hold_ms();
  test_wheel_while_held();
  test_hold_active_between_start_and_end();
  test_tap_only_key_coasting_wheel_stays_tap();
  test_tap_only_key_slow_hold_still_taps();
  test_turn_key_still_holds_and_rotates();
  return check_failures();
}
