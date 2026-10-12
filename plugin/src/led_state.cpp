// led_state.cpp — see led_state.h.
#include "led_state.h"

#include "speed_editor/key_map.h"  // LED_SNAP bit index

std::uint32_t MainLedMask(bool snapOn) {
  return snapOn ? (1u << LED_SNAP) : 0u;
}
