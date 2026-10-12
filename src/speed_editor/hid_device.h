#pragma once
// Ported from docs/speed-editor-examples/cpp-driver/src/hid_device.h
// (John-Howard/PythonSpeedEditor @ 28057b4, BSD-3-Clause — see LICENSE).
// Changes for speed-reaper (vibes/05-prototypes.md task 3):
//   - dropped the <utils/flog.h> (SDR++ logging) dependency
//   - setWheelMode() added; sendInitReports()/ledChaseDemo() removed
//     (the CLI initialises via setWheelMode(0); the named LED chase lives in the CLI)
//   - LED/mode writers return bool so the caller can report write failures
#include <cstdint>
#include <memory>
#include <string>
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
    // Authentication helpers
    static uint64_t rol8(uint64_t value);
    static uint64_t rol8n(uint64_t value, int n);
    static uint64_t calculateResponse(uint64_t challenge);

    void sendFeatureReport(const uint8_t* data, size_t len);
    int recvFeatureReport(uint8_t reportId, uint8_t* buf, size_t len);
    bool writeOutputReport(const uint8_t* data, size_t len);

    std::unique_ptr<IHidTransport> transport;
    bool opened = false;  // transport->Open() succeeded, not yet Close()d
    State state = State::Disconnected;
};
