#pragma once
// Seam under SpeedEditorHID (docs/plugin-oracle-design.md §6). The driver
// talks to a Speed Editor only through IHidTransport; HidapiTransport is
// the real implementation, a scripted FakeTransport is used by the oracle.
#include <cstddef>
#include <cstdint>
#include <string>

class IHidTransport {
public:
    virtual ~IHidTransport() = default;

    // Open the Speed Editor by VID/PID (optionally filter by serial number).
    // Returns true on success; Serial() is valid afterwards.
    virtual bool Open(const std::string &serial) = 0;
    virtual void Close() = 0;

    // Blocking read of one input report with timeout.
    // >0 bytes read, 0 timeout, -1 error/disconnect.
    virtual int Read(uint8_t *buf, size_t len, int timeout_ms) = 0;

    // Output reports (writes carry the report id as first byte).
    // Bytes written, or <0 on error.
    virtual int Write(const uint8_t *data, size_t len) = 0;

    // Feature report exchange (auth handshake, report 0x06).
    // Bytes written, or <0 on error.
    virtual int SendFeature(const uint8_t *data, size_t len) = 0;

    // Read a feature register; implementations set buf[0] = reportId
    // (hidapi convention), the device side fills the rest.
    // Bytes read, or <0 on error.
    virtual int GetFeature(uint8_t reportId, uint8_t *buf, size_t len) = 0;

    // Serial number of the open device ("" when closed).
    virtual std::string Serial() const = 0;
};
