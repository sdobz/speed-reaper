// led_state.h — main-panel LED bitmask from REAPER state (LED bar).
// Pure: no REAPER API, no HID. main.cpp's timer polls REAPER state into a
// LedState and pushes the mask via HidWheelSource::RequestLeds.
#ifndef SPEED_REAPER_PLUGIN_LED_STATE_H
#define SPEED_REAPER_PLUGIN_LED_STATE_H

#include <cstdint>

// Snapshot of the REAPER state mirrored on the main-panel LED bar.
struct LedState {
  bool snap = false;
  int ripple = 0;      // 0 off, 1 per-track, 2 all
  bool trim_behind = false;
  int active_take = 0; // 0 none/mixed, 1..9 shared by all selected items
  bool isolate = false;
  bool solo = false;
};

// Forward declarations for BindingLedMask (design § Mask): the bindings
// (bindings.h), the registry (verb_registry.h), and the live read context
// (LedReadContext, verb_registry.h).
struct Bindings;
struct VerbRegistry;
struct LedReadContext;

// Main-panel LED bitmask (bit indexes from speed_editor/key_map.h, LED_*):
//   SNAP = snap, CUT = ripple==1, DIS = ripple==2, LIVE_OWR = trim_behind,
//   CAM_n = active_take (1..9, via ledForCam), VIDEO_ONLY = isolate,
//   AUDIO_ONLY = solo.
std::uint32_t MainLedMask(const LedState& state);

// Main-panel LED bitmask driven by the bindings (design doc § Mask):
// for each of the 18 LED-bearing keys, take the first bound gesture in
// the order tap → double → turn → press; light the bit iff that
// binding's registry.StateReadFor(verb, args) is present and true
// against ctx. Unbound key or no read → dark. (Jog-mode LEDs, report
// 0x04, are a separate path.)
std::uint32_t BindingLedMask(const Bindings &bindings,
                             const VerbRegistry &registry,
                             const LedReadContext &ctx);

#endif  // SPEED_REAPER_PLUGIN_LED_STATE_H
