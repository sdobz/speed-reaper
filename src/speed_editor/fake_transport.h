#pragma once
// Scripted fake Speed Editor device (docs/plugin-oracle-design.md §6).
// Plays a list of timed events against IHidTransport so the plugin and
// driver can be tested without hardware (and on any OS):
//
//   at <ms> wheel <delta>    jog-wheel input report 0x03
//   at <ms> key <hex> down   key held (input report 0x04, full held set)
//   at <ms> key <hex> up     key released
//   at <ms> unplug           device disappears (Read → −1, Open fails)
//   at <ms> plug             device reappears (Open succeeds again)
//
// `at` times are milliseconds since the first successful Open. Blank lines
// and `#` comments are ignored. The fake also implements the device side
// of the report 0x06 auth handshake (algorithm: auth.h). No env selection
// lives here — the factory that picks the fake by env is a later task.
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "hid_transport.h"

struct ScriptEvent {
    enum class Kind { Wheel, KeyDown, KeyUp, Unplug, Plug };
    Kind kind = Kind::Wheel;
    long long atMs = 0;      // ms since Open
    int32_t delta = 0;       // Wheel only
    uint16_t keycode = 0;    // KeyDown/KeyUp only
};

// Parse a script (see above). On the first bad line fills *err (may be
// null) and returns an empty vector; otherwise returns the events.
std::vector<ScriptEvent> ParseFakeScript(const std::string &text, std::string *err);

class FakeTransport : public IHidTransport {
public:
    explicit FakeTransport(std::vector<ScriptEvent> events);

    bool Open(const std::string &serial) override;
    void Close() override;
    int Read(uint8_t *buf, size_t len, int timeout_ms) override;
    int Write(const uint8_t *data, size_t len) override;
    int SendFeature(const uint8_t *data, size_t len) override;
    int GetFeature(uint8_t reportId, uint8_t *buf, size_t len) override;
    std::string Serial() const override;

private:
    std::vector<ScriptEvent> events_;
    size_t nextIdx_ = 0;             // next event not yet released/consumed
    bool opened_ = false;
    bool unplugged_ = false;
    bool haveBaseline_ = false;      // first Open succeeded
    std::chrono::steady_clock::time_point baseline_;  // first successful Open
    uint8_t wheelMode_ = 0;          // from last Write of output report 0x03
    std::vector<uint16_t> heldKeys_;  // full held set, press order

    // Device side of the 0x06 handshake.
    enum class AuthStage {
        NeedChallenge,  // waiting for [0x06, 0x00]
        NeedAck,        // challenge generated, waiting for [0x06, 0x01]
        NeedResponse,   // acked, waiting for [0x06, 0x03, response]
        Accepted,
        Rejected,
    };
    AuthStage authStage_ = AuthStage::NeedChallenge;
    uint64_t challenge_ = 0;
};
