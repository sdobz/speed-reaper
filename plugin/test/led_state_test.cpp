// led_state_test.cpp — MainLedMask: SNAP on → LED_SNAP bit, off → 0.
#include "led_state.h"

#include "check.h"
#include "speed_editor/key_map.h"

int main() {
  CHECK(MainLedMask(true) == (1u << LED_SNAP));
  CHECK(MainLedMask(false) == 0u);
  // Only the SNAP bit, no others (guard against accidental extras).
  CHECK((MainLedMask(true) & ~(1u << LED_SNAP)) == 0u);
  return check_failures();
}
