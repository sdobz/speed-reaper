// Ported from docs/speed-editor-examples/cpp-driver/src/hid_device.cpp
// (John-Howard/PythonSpeedEditor @ 28057b4, BSD-3-Clause — see LICENSE).
// Changes for speed-reaper (vibes/05-prototypes.md task 3):
//   - dropped the <utils/flog.h> (SDR++ logging) dependency — failures go to stderr
//   - battery input report 0x07 is parsed (the vendored driver dropped it as Unknown)
//   - raw input-report bytes are attached to events (for `speedcli -v`)
//   - setWheelMode() added; sendInitReports()/ledChaseDemo() removed
// The auth handshake, tables, and report layouts are unchanged from the
// vendored driver and match docs/speed-editor-capabilities.md.
#include "hid_device.h"
#include "key_map.h"

#include <cstdio>
#include <cstring>

// ── Authentication tables (from smunaut's reverse-engineering) ──────

static const uint64_t AUTH_EVEN[] = {
    0x3AE1206F97C10BC8ULL, 0x2A9AB32BEBF244C6ULL,
    0x20A6F8B8DF9ADF0AULL, 0xAF80ECE52CFC1719ULL,
    0xEC2EE2F7414FD151ULL, 0xB055ADFD73344A15ULL,
    0xA63D2E3059001187ULL, 0x751BF623F42E0DDEULL,
};

static const uint64_t AUTH_ODD[] = {
    0x3E22B34F502E7FDEULL, 0x24656B981875AB1CULL,
    0xA17F3456DF7BF8C3ULL, 0x6DF72E1941AEF698ULL,
    0x72226F011E66AB94ULL, 0x3831A3C606296B42ULL,
    0xFD7FF81881332C89ULL, 0x61A3F6474FF236C6ULL,
};

static const uint64_t AUTH_MASK = 0xA79A63F585D37BF0ULL;

// ── Authentication math ────────────────────────────────────────────

uint64_t SpeedEditorHID::rol8(uint64_t value) {
    return ((value << 56) | (value >> 8)) & 0xFFFFFFFFFFFFFFFFULL;
}

uint64_t SpeedEditorHID::rol8n(uint64_t value, int n) {
    for (int i = 0; i < n; i++) {
        value = rol8(value);
    }
    return value;
}

uint64_t SpeedEditorHID::calculateResponse(uint64_t challenge) {
    int n = challenge & 7;
    uint64_t v = rol8n(challenge, n);
    uint64_t k;

    if ((v & 1) == ((0x78 >> n) & 1)) {
        k = AUTH_EVEN[n];
    }
    else {
        v ^= rol8(v);
        k = AUTH_ODD[n];
    }

    return v ^ (rol8(v) & AUTH_MASK) ^ k;
}

// ── Constructor / Destructor ───────────────────────────────────────

SpeedEditorHID::SpeedEditorHID() {}

SpeedEditorHID::~SpeedEditorHID() {
    close();
}

// ── Low-level HID I/O ─────────────────────────────────────────────

void SpeedEditorHID::sendFeatureReport(const uint8_t* data, size_t len) {
    if (!dev) { return; }
    if (hid_send_feature_report(dev, data, len) < 0) {
        std::fprintf(stderr, "speed_editor: send_feature_report failed\n");
    }
}

int SpeedEditorHID::recvFeatureReport(uint8_t reportId, uint8_t* buf, size_t len) {
    if (!dev) { return -1; }
    buf[0] = reportId;
    int res = hid_get_feature_report(dev, buf, len);
    if (res < 0) {
        std::fprintf(stderr, "speed_editor: get_feature_report failed\n");
    }
    return res;
}

bool SpeedEditorHID::writeOutputReport(const uint8_t* data, size_t len) {
    if (!dev) { return false; }
    if (hid_write(dev, data, len) < 0) {
        std::fprintf(stderr, "speed_editor: write failed\n");
        return false;
    }
    return true;
}

// ── Open / Close ───────────────────────────────────────────────────

bool SpeedEditorHID::open(const std::string& serialFilter) {
    close();

    state = State::Disconnected;

    // Enumerate to find the device
    struct hid_device_info* devs = hid_enumerate(SPEED_EDITOR_VID, SPEED_EDITOR_PID);
    if (!devs) {
        return false;
    }

    struct hid_device_info* target = nullptr;
    struct hid_device_info* cur = devs;
    while (cur) {
        if (!serialFilter.empty() && cur->serial_number) {
            // Convert wide string serial to std::string for comparison
            std::string devSerial;
            for (const wchar_t* p = cur->serial_number; *p; p++) {
                devSerial += (char)*p;
            }
            if (devSerial == serialFilter) {
                target = cur;
                break;
            }
        }
        else {
            // Take the first one found
            target = cur;
            break;
        }
        cur = cur->next;
    }

    if (!target) {
        hid_free_enumeration(devs);
        return false;
    }

    dev = hid_open_path(target->path);
    if (!dev) {
        hid_free_enumeration(devs);
        return false;
    }

    // Store serial number
    if (target->serial_number) {
        serial.clear();
        for (const wchar_t* p = target->serial_number; *p; p++) {
            serial += (char)*p;
        }
    }

    hid_free_enumeration(devs);
    return true;
}

void SpeedEditorHID::close() {
    if (dev) {
        allLedsOff();
        hid_close(dev);
        dev = nullptr;
    }
    state = State::Disconnected;
    serial.clear();
}

// ── Authentication ─────────────────────────────────────────────────

bool SpeedEditorHID::authenticate() {
    if (!dev) { return false; }

    state = State::Authenticating;

    // Step 1 — request challenge
    uint8_t req[10] = {0x06, 0x00};
    sendFeatureReport(req, sizeof(req));

    uint8_t challengeBuf[10] = {};
    if (recvFeatureReport(0x06, challengeBuf, sizeof(challengeBuf)) < 0) {
        state = State::Disconnected;
        return false;
    }

    // Step 2 — acknowledge
    uint8_t ack[10] = {0x06, 0x01};
    sendFeatureReport(ack, sizeof(ack));

    uint8_t ackResp[10] = {};
    recvFeatureReport(0x06, ackResp, sizeof(ackResp));

    // Step 3 — compute and send response
    uint64_t challenge;
    memcpy(&challenge, &challengeBuf[2], sizeof(uint64_t));
    uint64_t response = calculateResponse(challenge);

    uint8_t respReport[10] = {0x06, 0x03};
    memcpy(&respReport[2], &response, sizeof(uint64_t));
    sendFeatureReport(respReport, sizeof(respReport));

    // Step 4 — verify acceptance
    uint8_t result[10] = {};
    if (recvFeatureReport(0x06, result, sizeof(result)) < 0) {
        state = State::Disconnected;
        return false;
    }

    if (result[0] != 0x06 || result[1] != 0x04) {
        std::fprintf(stderr, "speed_editor: authentication FAILED\n");
        state = State::Disconnected;
        return false;
    }

    state = State::Connected;
    return true;
}

// ── Wheel mode ─────────────────────────────────────────────────────

bool SpeedEditorHID::setWheelMode(uint8_t mode) {
    if (!dev) { return false; }
    // Output report 0x03: [id, mode, u32 0, u8 0xFF].
    // The 0xFF tail is the form used by python-bmd/bmd.py set_jog_mode()
    // ("unknown byte that works"); the vendored driver used 0x00 — both work.
    uint8_t report[] = {0x03, mode, 0x00, 0x00, 0x00, 0x00, 0xFF};
    return writeOutputReport(report, sizeof(report));
}

// ── Event reading ──────────────────────────────────────────────────

bool SpeedEditorHID::readEvent(SpeedEditorEvent& event, int timeoutMs) {
    if (!dev) { return false; }

    uint8_t buf[64] = {};
    int res = hid_read_timeout(dev, buf, sizeof(buf), timeoutMs);
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
    if (!dev) { return; }
    uint8_t leds[] = {0x02, 0x00, 0x00, 0x00, 0x00};
    hid_write(dev, leds, sizeof(leds));
    uint8_t jog[] = {0x04, 0x00};
    hid_write(dev, jog, sizeof(jog));
}
