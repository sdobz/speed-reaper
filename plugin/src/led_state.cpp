// led_state.cpp — see led_state.h.
#include "led_state.h"

#include "speed_editor/key_map.h"  // LED_* bit indexes, ledForCam

std::uint32_t MainLedMask(const LedState& s) {
  std::uint32_t mask = 0u;
  auto set = [&mask](int bit) { mask |= (1u << bit); };
  if (s.snap) set(LED_SNAP);
  if (s.ripple == 1) set(LED_CUT);
  if (s.ripple == 2) set(LED_DIS);
  if (s.trim_behind) set(LED_LIVE_OWR);
  const int cam = ledForCam(s.active_take);
  if (cam >= 0) set(cam);
  if (s.isolate) set(LED_VIDEO_ONLY);
  if (s.solo) set(LED_AUDIO_ONLY);
  return mask;
}
