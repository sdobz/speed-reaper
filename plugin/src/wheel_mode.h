// wheel_mode.h — wheel behavior modes for the Speed Editor.
//
// The physical wheel can jog the playhead (Jog) or scroll the arrange view
// horizontally (Scrl); Shtl is the third physical mode (shuttle: its
// absolute offset sets the shuttle rate). ProtocolMode() maps a mode to the
// wheel-mode value the device expects (ABSOLUTE_DEADZERO report):
// Jog=0, Scrl=2, Shtl=3.

#ifndef SPEED_REAPER_WHEEL_MODE_H
#define SPEED_REAPER_WHEEL_MODE_H

#include <cstdint>

enum class WheelMode { Jog, Shtl, Scrl };

// Device protocol value for a mode: Jog=0, Scrl=2, Shtl=3.
inline std::uint8_t ProtocolMode(WheelMode m) {
  switch (m) {
    case WheelMode::Jog: return 0;
    case WheelMode::Scrl: return 2;
    case WheelMode::Shtl: return 3;
  }
  return 0;
}

#endif  // SPEED_REAPER_WHEEL_MODE_H
