#pragma once
// Real IHidTransport: the hidapi calls that SpeedEditorHID used to make
// directly (vibes/19-oracle-device-windows.md task 1). Pure wrapper —
// diagnostics and state machine stay in the driver.
#include <hidapi/hidapi.h>
#include <string>
#include "hid_transport.h"

class HidapiTransport final : public IHidTransport {
public:
    bool Open(const std::string &serial) override;
    void Close() override;
    int Read(uint8_t *buf, size_t len, int timeout_ms) override;
    int Write(const uint8_t *data, size_t len) override;
    int SendFeature(const uint8_t *data, size_t len) override;
    int GetFeature(uint8_t reportId, uint8_t *buf, size_t len) override;
    std::string Serial() const override;

private:
    hid_device* dev = nullptr;
    std::string serial;
};
