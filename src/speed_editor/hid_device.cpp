// Ported from docs/speed-editor-examples/cpp-driver/src/hid_device.cpp
// (John-Howard/PythonSpeedEditor @ 28057b4, BSD-3-Clause — see LICENSE).
// Changes for speed-reaper (vibes/05-prototypes.md task 3):
//   - dropped the <utils/flog.h> (SDR++ logging) dependency — failures go to stderr
//   - battery input report 0x07 is parsed (the vendored driver dropped it as Unknown)
//   - raw input-report bytes are attached to events (for `speedcli -v`)
//   - setWheelMode() added; sendInitReports()/ledChaseDemo() removed
//   - auth math moved to auth.h (free functions, shared with FakeTransport)
// The auth handshake and report layouts are unchanged from the vendored
// driver and match docs/speed-editor-capabilities.md.
#include "hid_device.h"
#include "hidapi_transport.h"

#include <cstdio>
#include <cstring>

namespace {
// Re-auth timeout (s) used when the device's status report carries no
// usable value (fake device reports 0). Observed on real hardware: 600.
constexpr int kDefaultReauthTimeoutSec = 600;
}  // namespace

// ── Constructor / Destructor ───────────────────────────────────────

SpeedEditorHID::SpeedEditorHID(std::unique_ptr<IHidTransport> transportIn)
    : transport(std::move(transportIn)) {
    if (!transport) {
        transport = std::make_unique<HidapiTransport>();
    }
}

SpeedEditorHID::~SpeedEditorHID() {
    close();
}

// ── Low-level HID I/O ─────────────────────────────────────────────

void SpeedEditorHID::sendFeatureReport(const uint8_t* data, size_t len) {
    if (!opened) { return; }
    if (transport->SendFeature(data, len) < 0) {
        std::fprintf(stderr, "speed_editor: send_feature_report failed\n");
    }
}

int SpeedEditorHID::recvFeatureReport(uint8_t reportId, uint8_t* buf, size_t len) {
    if (!opened) { return -1; }
    int res = transport->GetFeature(reportId, buf, len);
    if (res < 0) {
        std::fprintf(stderr, "speed_editor: get_feature_report failed\n");
    }
    return res;
}

bool SpeedEditorHID::writeOutputReport(const uint8_t* data, size_t len) {
    if (!opened) { return false; }
    if (transport->Write(data, len) < 0) {
        std::fprintf(stderr, "speed_editor: write failed\n");
        return false;
    }
    return true;
}

// ── Open / Close ───────────────────────────────────────────────────

bool SpeedEditorHID::open(const std::string& serialFilter) {
    close();

    state = State::Disconnected;

    if (!transport->Open(serialFilter)) {
        return false;
    }

    opened = true;
    return true;
}

void SpeedEditorHID::close() {
    if (opened) {
        allLedsOff();
        transport->Close();
        opened = false;
    }
    state = State::Disconnected;
}

// ── Authentication ─────────────────────────────────────────────────

int SpeedEditorHID::authenticate() {
    if (!opened) { return 0; }

    state = State::Authenticating;

    // Step 1 — request challenge
    uint8_t req[10] = {0x06, 0x00};
    sendFeatureReport(req, sizeof(req));

    uint8_t challengeBuf[10] = {};
    if (recvFeatureReport(0x06, challengeBuf, sizeof(challengeBuf)) < 0) {
        state = State::Disconnected;
        return 0;
    }

    // Step 2 — acknowledge
    uint8_t ack[10] = {0x06, 0x01};
    sendFeatureReport(ack, sizeof(ack));

    uint8_t ackResp[10] = {};
    recvFeatureReport(0x06, ackResp, sizeof(ackResp));

    // Step 3 — compute and send response
    uint64_t challenge;
    memcpy(&challenge, &challengeBuf[2], sizeof(uint64_t));
    uint64_t response = speed_editor_auth::calculateResponse(challenge);

    uint8_t respReport[10] = {0x06, 0x03};
    memcpy(&respReport[2], &response, sizeof(uint64_t));
    sendFeatureReport(respReport, sizeof(respReport));

    // Step 4 — verify acceptance
    uint8_t result[10] = {};
    if (recvFeatureReport(0x06, result, sizeof(result)) < 0) {
        state = State::Disconnected;
        return 0;
    }

    if (result[0] != 0x06 || result[1] != 0x04) {
        std::fprintf(stderr, "speed_editor: authentication FAILED\n");
        state = State::Disconnected;
        return 0;
    }

    state = State::Connected;
    // Status payload: u16 LE re-auth timeout (seconds; observed 600).
    uint16_t timeout_sec = 0;
    std::memcpy(&timeout_sec, &result[2], sizeof(timeout_sec));
    return timeout_sec > 0
        ? static_cast<int>(timeout_sec)
        : kDefaultReauthTimeoutSec;
}

// ── Wheel mode ─────────────────────────────────────────────────────

bool SpeedEditorHID::setWheelMode(uint8_t mode) {
    if (!opened) { return false; }
    // Output report 0x03: [id, mode, u32 0, u8 0xFF].
    // The 0xFF tail is the form used by python-bmd/bmd.py set_jog_mode()
    // ("unknown byte that works"); the vendored driver used 0x00 — both work.
    uint8_t report[] = {0x03, mode, 0x00, 0x00, 0x00, 0x00, 0xFF};
    return writeOutputReport(report, sizeof(report));
}

// ── Event reading ──────────────────────────────────────────────────

bool SpeedEditorHID::readEvent(SpeedEditorEvent& event, int timeoutMs) {
    if (!opened) { return false; }

    uint8_t buf[64] = {};
    int res = transport->Read(buf, sizeof(buf), timeoutMs);
    if (res == 0) { return false; }  // timeout — no data
    if (res < 0) {
        // Read error — device likely disconnected
        std::fprintf(stderr, "speed_editor: read error, closing device\n");
        close();
        return false;
    }

    uint8_t reportId = buf[0];
    event.raw.assign(buf, buf + res);

    if (reportId == 0x03 && res >= 6) {
        // Jog wheel: [0x03, mode, delta_i32_le, ...unknown tail]
        event.type = EventType::Wheel;
        event.wheel.mode = buf[1];
        int32_t delta;
        memcpy(&delta, &buf[2], sizeof(int32_t));
        event.wheel.delta = delta;
        return true;
    }

    if (reportId == 0x04) {
        // Button report: up to 6 keycodes as uint16_le (full held set)
        event.type = EventType::Button;
        event.button.keycodes.clear();
        for (int i = 0; i < 6; i++) {
            int offset = 1 + i * 2;
            if (offset + 1 < res) {
                uint16_t kc;
                memcpy(&kc, &buf[offset], sizeof(uint16_t));
                if (kc != 0) {
                    event.button.keycodes.push_back(kc);
                }
            }
        }
        return true;
    }

    if (reportId == 0x07 && res >= 3) {
        // Battery: [0x07, charging (0/1), level (0-100)]
        event.type = EventType::Battery;
        event.battery.charging = buf[1] != 0;
        event.battery.level = buf[2];
        return true;
    }

    event.type = EventType::Unknown;
    return true;
}

// ── LED control ────────────────────────────────────────────────────

bool SpeedEditorHID::setLeds(uint8_t b0, uint8_t b1, uint8_t b2) {
    uint8_t report[] = {0x02, b0, b1, b2, 0x00};
    return writeOutputReport(report, sizeof(report));
}

bool SpeedEditorHID::setJogLeds(bool jog, bool shtl, bool scrl) {
    uint8_t val = (jog ? 1 : 0) | (shtl ? 2 : 0) | (scrl ? 4 : 0);
    uint8_t report[] = {0x04, val};
    return writeOutputReport(report, sizeof(report));
}

void SpeedEditorHID::allLedsOff() {
    // Best-effort: ignore failures (e.g. the device was just unplugged).
    if (!opened) { return; }
    uint8_t leds[] = {0x02, 0x00, 0x00, 0x00, 0x00};
    transport->Write(leds, sizeof(leds));
    uint8_t jog[] = {0x04, 0x00};
    transport->Write(jog, sizeof(jog));
}
