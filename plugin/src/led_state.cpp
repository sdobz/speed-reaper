// led_state.cpp — see led_state.h.
#include "led_state.h"

#include "bindings.h"  // Bindings / BoundVerb
#include "speed_editor/key_map.h"  // KEY_*/LED_* key codes and bit indexes
#include "verb_registry.h"  // VerbRegistry, BindingGesture, LedReadContext

namespace {

// Key → LED bit for the 18 LED-bearing keys (LED_* from key_map.h).
const std::map<std::uint16_t, int> &KeyLedBits() {
  static const std::map<std::uint16_t, int> bits = {
      {KEY_CLOSE_UP, LED_CLOSE_UP},
      {KEY_CUT, LED_CUT},
      {KEY_DIS, LED_DIS},
      {KEY_SMTH_CUT, LED_SMTH_CUT},
      {KEY_TRANS, LED_TRANS},
      {KEY_SNAP, LED_SNAP},
      {KEY_LIVE_OWR, LED_LIVE_OWR},
      {KEY_VIDEO_ONLY, LED_VIDEO_ONLY},
      {KEY_AUDIO_ONLY, LED_AUDIO_ONLY},
      {KEY_CAM1, LED_CAM1},
      {KEY_CAM2, LED_CAM2},
      {KEY_CAM3, LED_CAM3},
      {KEY_CAM4, LED_CAM4},
      {KEY_CAM5, LED_CAM5},
      {KEY_CAM6, LED_CAM6},
      {KEY_CAM7, LED_CAM7},
      {KEY_CAM8, LED_CAM8},
      {KEY_CAM9, LED_CAM9},
  };
  return bits;
}

// The LED's binding order (design § Mask): the first bound gesture in
// this order wins; the rest are never consulted.
const BindingGesture kLedGestureOrder[] = {
    BindingGesture::Tap, BindingGesture::Double, BindingGesture::Turn,
    BindingGesture::Press};

}  // namespace

std::uint32_t BindingLedMask(const Bindings &bindings,
                             const VerbRegistry &registry,
                             const LedReadContext &ctx) {
  std::uint32_t mask = 0u;
  for (const auto &kb : KeyLedBits()) {
    const BoundVerb *bound = nullptr;
    for (const BindingGesture g : kLedGestureOrder)
      if (const auto it = bindings.bindings.find({kb.first, g});
          it != bindings.bindings.end()) {
        bound = &it->second;
        break;
      }
    if (bound == nullptr) continue;  // unbound key → dark
    const auto read = registry.StateReadFor(bound->name, bound->args);
    if (read && (*read)(ctx)) mask |= (1u << kb.second);
  }
  return mask;
}
