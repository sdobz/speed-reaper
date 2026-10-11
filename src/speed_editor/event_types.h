#pragma once
// Ported from docs/speed-editor-examples/cpp-driver/src/event_types.h
// (John-Howard/PythonSpeedEditor @ 28057b4, BSD-3-Clause — see LICENSE).
// Changes for speed-reaper (vibes/05-prototypes.md task 3):
//   - BatteryEvent added (input report 0x07)
//   - SpeedEditorEvent carries the raw report bytes (for `speedcli -v`)
#include <cstdint>
#include <vector>

enum class EventType {
    Wheel,
    Button,
    Battery,
    Unknown,
};

struct WheelEvent {
    uint8_t mode;   // protocol mode byte 0-3 (capabilities doc §2.1)
    int32_t delta;  // signed; motion since last report (relative modes)
};

struct ButtonEvent {
    std::vector<uint16_t> keycodes;  // full held-key set, 0-filtered
    bool isRelease() const { return keycodes.empty(); }
};

struct BatteryEvent {
    bool charging;  // USB power present
    int level;      // 0-100
};

struct SpeedEditorEvent {
    EventType type;
    WheelEvent wheel{};
    ButtonEvent button;
    BatteryEvent battery{};
    std::vector<uint8_t> raw;  // full input report, report ID byte first
};
