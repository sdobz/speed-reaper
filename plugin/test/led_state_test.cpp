// led_state_test.cpp — MainLedMask(const LedState&): design §5 bit mapping.
#include "led_state.h"

#include "check.h"
#include "speed_editor/key_map.h"

namespace {

constexpr std::uint32_t Bit(int led) { return 1u << led; }

}  // namespace

int main() {
  // Default state: nothing lit.
  CHECK(MainLedMask(LedState{}) == 0u);

  // Snap only.
  {
    LedState s;
    s.snap = true;
    CHECK(MainLedMask(s) == Bit(LED_SNAP));
  }

  // Ripple: per-track -> CUT only; all -> DIS only; off -> none.
  {
    LedState s;
    s.ripple = 1;
    CHECK(MainLedMask(s) == Bit(LED_CUT));
    s.ripple = 2;
    CHECK(MainLedMask(s) == Bit(LED_DIS));
    s.ripple = 0;
    CHECK(MainLedMask(s) == 0u);
  }

  // Trim-behind -> LIVE_OWR.
  {
    LedState s;
    s.trim_behind = true;
    CHECK(MainLedMask(s) == Bit(LED_LIVE_OWR));
  }

  // Active take: 2 -> CAM2 only; 0 -> none; out of range -> none.
  {
    LedState s;
    s.active_take = 2;
    CHECK(MainLedMask(s) == Bit(LED_CAM2));
    s.active_take = 0;
    CHECK(MainLedMask(s) == 0u);
    s.active_take = 12;
    CHECK(MainLedMask(s) == 0u);
  }

  // Isolate / solo bits.
  {
    LedState s;
    s.isolate = true;
    CHECK(MainLedMask(s) == Bit(LED_VIDEO_ONLY));
    s.isolate = false;
    s.solo = true;
    CHECK(MainLedMask(s) == Bit(LED_AUDIO_ONLY));
  }

  // Combinations OR together (and stay within the known LED bits).
  {
    LedState s;
    s.snap = true;
    s.ripple = 1;
    s.trim_behind = true;
    s.active_take = 7;
    s.isolate = true;
    s.solo = true;
    const std::uint32_t expected =
        Bit(LED_SNAP) | Bit(LED_CUT) | Bit(LED_LIVE_OWR) | Bit(LED_CAM7) |
        Bit(LED_VIDEO_ONLY) | Bit(LED_AUDIO_ONLY);
    CHECK(MainLedMask(s) == expected);
  }
  return check_failures();
}
