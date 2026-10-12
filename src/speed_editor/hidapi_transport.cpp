// Real IHidTransport (hidapi) — the enumeration and I/O calls moved out of
// SpeedEditorHID verbatim (vibes/19-oracle-device-windows.md task 1).
#include "hidapi_transport.h"

#include <cstdint>

#include "key_map.h"  // SPEED_EDITOR_VID/PID

bool HidapiTransport::Open(const std::string& serialFilter) {
    Close();

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

void HidapiTransport::Close() {
    if (dev) {
        hid_close(dev);
        dev = nullptr;
    }
    serial.clear();
}

int HidapiTransport::Read(uint8_t* buf, size_t len, int timeout_ms) {
    if (!dev) { return -1; }
    return hid_read_timeout(dev, buf, len, timeout_ms);
}

int HidapiTransport::Write(const uint8_t* data, size_t len) {
    if (!dev) { return -1; }
    return hid_write(dev, data, len);
}

int HidapiTransport::SendFeature(const uint8_t* data, size_t len) {
    if (!dev) { return -1; }
    return hid_send_feature_report(dev, data, len);
}

int HidapiTransport::GetFeature(uint8_t reportId, uint8_t* buf, size_t len) {
    if (!dev) { return -1; }
    buf[0] = reportId;  // hidapi convention: report id travels in buf[0]
    return hid_get_feature_report(dev, buf, len);
}

std::string HidapiTransport::Serial() const {
    return serial;
}
