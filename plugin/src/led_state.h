// led_state.h — main-panel LED bitmask from REAPER state (M3 edit verbs).
// Pure: no REAPER API, no HID. The timer in main.cpp polls the snap toggle
// (command 1157) and pushes the mask via HidWheelSource::RequestLeds.
#ifndef SPEED_REAPER_PLUGIN_LED_STATE_H
#define SPEED_REAPER_PLUGIN_LED_STATE_H

#include <cstdint>

// Main-panel LED bitmask with exactly the SNAP LED set when `snapOn`
// (bit index LED_SNAP from speed_editor/key_map.h). No other LEDs.
std::uint32_t MainLedMask(bool snapOn);

#endif  // SPEED_REAPER_PLUGIN_LED_STATE_H
