#pragma once
// Ported from docs/speed-editor-examples/cpp-driver/src/hid_device.h
// (John-Howard/PythonSpeedEditor @ 28057b4, BSD-3-Clause — see LICENSE).
// Changes for speed-reaper (vibes/05-prototypes.md task 3):
//   - dropped the <utils/flog.h> (SDR++ logging) dependency
//   - setWheelMode() added; sendInitReports()/ledChaseDemo() removed
//     (the CLI initialises via setWheelMode(0); the named LED chase lives in the CLI)
//   - LED/mode writers return bool so the caller can report write failures
//   - LedsReportBytes(): single source of truth for the report 0x02 byte
//     split (regression: a 6-bit group split lit the wrong physical LEDs)
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include "auth.h"
#include "event_types.h"
#include "hid_transport.h"

class SpeedEditorHID {
public:
    enum class State {
        Disconnected,
        Authenticating,
        Connected,
    };

    // transport: pluggable device backend (default: real hidapi device).
    SpeedEditorHID(std::unique_ptr<IHidTransport> transport = nullptr);
    ~SpeedEditorHID();

    // Open device by VID/PID (optionally filter by serial number).
    // Returns true on success.
    bool open(const std::string& serialFilter = "");
    void close();

    // Run the challenge-response authentication handshake (report 0x06).
    // The device sends no input reports until this succeeds.
    bool authenticate();

    // Set the wheel protocol mode (output report 0x03). mode is 0-3.
    bool setWheelMode(uint8_t mode);

    // Read one event with timeout. Returns false on timeout (no data).
    // A device disconnect also yields false and leaves the device closed.
    bool readEvent(SpeedEditorEvent& event, int timeoutMs = 100);

    // LED control. The two LED subsystems are separate output reports.
    bool setLeds(uint8_t b0, uint8_t b1, uint8_t b2);       // report 0x02, 18 main-panel LEDs
    bool setJogLeds(bool jog, bool shtl, bool scrl);        // report 0x04, 3 mode LEDs
    void allLedsOff();

    State getState() const { return state; }
    std::string getSerial() const { return transport->Serial(); }

private:
    void sendFeatureReport(const uint8_t* data, size_t len);
    int recvFeatureReport(uint8_t reportId, uint8_t* buf, size_t len);
    bool writeOutputReport(const uint8_t* data, size_t len);

    std::unique_ptr<IHidTransport> transport;
    bool opened = false;  // transport->Open() succeeded, not yet Close()d
    State state = State::Disconnected;
};

// Split the 18-LED main-panel mask (bit index = LED_* in key_map.h) into
// the three payload bytes of output report 0x02. The report is an LE32
// bitfield — [0x02, b0, b1, b2, 0x00] — so b0 = bits 0–7, b1 = bits 8–15,
// b2 = bits 16–17. NOT 6-bit groups: bit i lives in byte i/8 at position
// i%8 (a 6-bit split sends bit 8 to device bit 10, lighting CAM4 when
// CAM9 was meant — found on real hardware, 45-cam-n field test).
// The device defines 18 LED bits; anything above bit 17 is dropped so the
// output is always a valid report 0x02 payload.
inline std::array<uint8_t, 3> LedsReportBytes(std::uint32_t mask) {
    const std::uint32_t m = mask & 0x3FFFFu;
    return {static_cast<uint8_t>(m & 0xFFu),
            static_cast<uint8_t>((m >> 8) & 0xFFu),
            static_cast<uint8_t>((m >> 16) & 0xFFu)};
}
