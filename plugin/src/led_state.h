// led_state.h — main-panel LED bitmask from REAPER state (LED bar).
// Pure: no REAPER API, no HID. main.cpp's timer polls REAPER state and
// pushes the mask via HidWheelSource::RequestLeds.
#ifndef SPEED_REAPER_PLUGIN_LED_STATE_H
#define SPEED_REAPER_PLUGIN_LED_STATE_H

#include <cstdint>

// Forward declarations for BindingLedMask (design § Mask): the bindings
// (bindings.h), the registry (verb_registry.h), and the live read context
// (LedReadContext, verb_registry.h).
struct Bindings;
struct VerbRegistry;
struct LedReadContext;

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
