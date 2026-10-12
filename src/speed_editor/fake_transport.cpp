// Scripted fake Speed Editor device (docs/plugin-oracle-design.md §6).
// See fake_transport.h for the script grammar. Time base: elapsed wall
// time (steady_clock) since the first successful Open. Events are not
// re-armed on reopen — the script plays once, so `unplug`/`plug` lines
// refer to one cycle.
#include "auth.h"
#include "fake_transport.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>

namespace {

using Clock = std::chrono::steady_clock;

bool parseLong(const std::string &s, long long *out) {
    if (s.empty()) { return false; }
    errno = 0;
    char *end = nullptr;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno == ERANGE || end == s.c_str() || *end != '\0') { return false; }
    *out = v;
    return true;
}

bool parseHex16(const std::string &s, uint16_t *out) {
    if (s.size() < 2) { return false; }
    size_t start = (s.rfind("0x", 0) == 0) ? 2 : 0;
    if (s.size() <= start) { return false; }
    errno = 0;
    char *end = nullptr;
    unsigned long v = std::strtoul(s.c_str() + start, &end, 16);
    if (errno == ERANGE || end != s.c_str() + s.size() || v > 0xFFFF) { return false; }
    *out = static_cast<uint16_t>(v);
    return true;
}

std::string trim(const std::string &s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) { b++; }
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) { e--; }
    return s.substr(b, e - b);
}

}  // namespace

std::vector<ScriptEvent> ParseFakeScript(const std::string &text, std::string *err) {
    auto fail = [&](const std::string &msg) {
        if (err) { *err = msg; }
        return std::vector<ScriptEvent>{};
    };

    std::vector<ScriptEvent> out;
    size_t lineNo = 0;
    std::istringstream stream(text);
    std::string line;
    // `|| !line.empty()` keeps a final line that lacks a trailing newline.
    while (std::getline(stream, line) || !line.empty()) {
        lineNo++;

        size_t hash = line.find('#');
        if (hash != std::string::npos) { line.erase(hash); }
        if (trim(line).empty()) { continue; }

        std::istringstream in(line);
        std::vector<std::string> tok;
        for (std::string t; in >> t; ) { tok.push_back(t); }

        if (tok.size() < 3 || tok[0] != "at") {
            return fail("line " + std::to_string(lineNo) + ": expected 'at <ms> ...'");
        }
        ScriptEvent e;
        if (!parseLong(tok[1], &e.atMs) || e.atMs < 0) {
            return fail("line " + std::to_string(lineNo) + ": bad time '" + tok[1] + "'");
        }
        const std::string &verb = tok[2];
        if (verb == "wheel") {
            if (tok.size() != 4) {
                return fail("line " + std::to_string(lineNo) + ": 'wheel <delta>' needs one argument");
            }
            long long delta = 0;
            if (!parseLong(tok[3], &delta)) {
                return fail("line " + std::to_string(lineNo) + ": bad wheel delta '" + tok[3] + "'");
            }
            e.delta = static_cast<int32_t>(delta);
            e.kind = ScriptEvent::Kind::Wheel;
        }
        else if (verb == "key") {
            if (tok.size() != 5) {
                return fail("line " + std::to_string(lineNo) + ": 'key <hex> down|up' needs two arguments");
            }
            if (!parseHex16(tok[3], &e.keycode)) {
                return fail("line " + std::to_string(lineNo) + ": bad keycode '" + tok[3] + "'");
            }
            if (tok[4] == "down") {
                e.kind = ScriptEvent::Kind::KeyDown;
            }
            else if (tok[4] == "up") {
                e.kind = ScriptEvent::Kind::KeyUp;
            }
            else {
                return fail("line " + std::to_string(lineNo) + ": bad key state '" + tok[4] + "'");
            }
        }
        else if (verb == "unplug") {
            if (tok.size() != 3) {
                return fail("line " + std::to_string(lineNo) + ": 'unplug' takes no arguments");
            }
            e.kind = ScriptEvent::Kind::Unplug;
        }
        else if (verb == "plug") {
            if (tok.size() != 3) {
                return fail("line " + std::to_string(lineNo) + ": 'plug' takes no arguments");
            }
            e.kind = ScriptEvent::Kind::Plug;
        }
        else {
            return fail("line " + std::to_string(lineNo) + ": unknown verb '" + verb + "'");
        }
        out.push_back(e);
    }
    return out;
}

FakeTransport::FakeTransport(std::vector<ScriptEvent> events)
    : events_(std::move(events)) {}

bool FakeTransport::Open(const std::string &serial) {
    (void)serial;
    if (unplugged_) {
        // Device reappears when the matching `plug` time has elapsed
        // (measured from the original Open).
        for (size_t i = nextIdx_; i < events_.size(); ++i) {
            if (events_[i].kind == ScriptEvent::Kind::Plug) {
                bool due = haveBaseline_ &&
                    Clock::now() >= baseline_ + std::chrono::milliseconds(events_[i].atMs);
                if (due) {
                    nextIdx_ = i + 1;
                    unplugged_ = false;
                }
                break;  // the matching plug has been decided
            }
        }
        if (unplugged_) { return false; }
    }
    if (!haveBaseline_) {
        baseline_ = Clock::now();
        haveBaseline_ = true;
    }
    // Reconnect = fresh device state (like re-enumeration).
    wheelMode_ = 0;
    heldKeys_.clear();
    authStage_ = AuthStage::NeedChallenge;
    opened_ = true;
    return true;
}

void FakeTransport::Close() {
    opened_ = false;
}

int FakeTransport::Read(uint8_t *buf, size_t len, int timeout_ms) {
    auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        if (!opened_ || unplugged_) { return -1; }
        if (authStage_ != AuthStage::Accepted) {
            // The device sends no input reports until auth succeeds.
            auto now = Clock::now();
            if (now < deadline) { std::this_thread::sleep_for(deadline - now); }
            return 0;
        }
        if (nextIdx_ >= events_.size()) {
            auto now = Clock::now();
            if (now < deadline) { std::this_thread::sleep_for(deadline - now); }
            return 0;  // script exhausted — timeout
        }
        const ScriptEvent &e = events_[nextIdx_];
        auto due = baseline_ + std::chrono::milliseconds(e.atMs);
        auto now = Clock::now();
        if (now < due) {
            auto limit = (due < deadline) ? due : deadline;
            std::this_thread::sleep_for(limit - now);
            if (Clock::now() >= deadline) { return 0; }  // timed out before due
            continue;
        }
        switch (e.kind) {
            case ScriptEvent::Kind::Wheel: {
                nextIdx_++;
                uint8_t rep[6] = {0x03, wheelMode_, 0x00, 0x00, 0x00, 0x00};
                std::memcpy(&rep[2], &e.delta, sizeof(int32_t));
                size_t n = std::min<size_t>(sizeof(rep), len);
                std::memcpy(buf, rep, n);
                return static_cast<int>(n);
            }
            case ScriptEvent::Kind::KeyDown:
                if (std::find(heldKeys_.begin(), heldKeys_.end(), e.keycode) == heldKeys_.end()) {
                    heldKeys_.push_back(e.keycode);
                }
                break;
            case ScriptEvent::Kind::KeyUp:
                heldKeys_.erase(
                    std::remove(heldKeys_.begin(), heldKeys_.end(), e.keycode), heldKeys_.end());
                break;
            case ScriptEvent::Kind::Unplug:
                nextIdx_++;
                unplugged_ = true;
                return -1;
            case ScriptEvent::Kind::Plug:
                // Stray plug without a preceding unplug — consume as a no-op.
                nextIdx_++;
                continue;
        }
        // KeyDown/KeyUp: emit the full held set as input report 0x04.
        nextIdx_++;
        std::vector<uint8_t> rep(1, 0x04);
        for (size_t i = 0; i < heldKeys_.size() && i < 6; i++) {
            rep.push_back(static_cast<uint8_t>(heldKeys_[i] & 0xFF));
            rep.push_back(static_cast<uint8_t>(heldKeys_[i] >> 8));
        }
        size_t n = std::min(rep.size(), len);
        std::memcpy(buf, rep.data(), n);
        return static_cast<int>(n);
    }
}

int FakeTransport::Write(const uint8_t *data, size_t len) {
    if (!opened_ || unplugged_ || len == 0) { return -1; }
    if (data[0] == 0x03 && len >= 2) {
        wheelMode_ = data[1];  // wheel protocol mode (input reports echo it)
    }
    // Other output reports (0x02 LEDs, 0x04 jog LEDs) are accepted and ignored.
    return static_cast<int>(len);
}

int FakeTransport::SendFeature(const uint8_t *data, size_t len) {
    if (!opened_ || unplugged_ || len < 2 || data[0] != 0x06) { return -1; }
    switch (data[1]) {
        case 0x00: {  // challenge request (re-arms the handshake)
            uint64_t c = std::random_device{}() ^
                static_cast<uint64_t>(Clock::now().time_since_epoch().count());
            challenge_ = (c == 0) ? 1 : c;
            authStage_ = AuthStage::NeedAck;
            break;
        }
        case 0x01:  // acknowledge
            if (authStage_ == AuthStage::NeedAck) {
                authStage_ = AuthStage::NeedResponse;
            }
            break;
        case 0x03: {  // response
            if (authStage_ == AuthStage::NeedResponse && len >= 10) {
                uint64_t resp = 0;
                std::memcpy(&resp, &data[2], sizeof(uint64_t));
                authStage_ = (resp == speed_editor_auth::calculateResponse(challenge_))
                    ? AuthStage::Accepted
                    : AuthStage::Rejected;
            }
            break;
        }
        default:
            break;
    }
    return static_cast<int>(len);
}

int FakeTransport::GetFeature(uint8_t reportId, uint8_t *buf, size_t len) {
    if (!opened_ || unplugged_ || reportId != 0x06 || len < 10) { return -1; }
    uint8_t out[10] = {};
    out[0] = 0x06;
    switch (authStage_) {
        case AuthStage::NeedAck:
            out[1] = 0x01;  // challenge follows in bytes 2..9
            std::memcpy(&out[2], &challenge_, sizeof(uint64_t));
            break;
        case AuthStage::NeedResponse:
            out[1] = 0x02;  // ack receipt (host ignores the payload)
            break;
        case AuthStage::Accepted:
            out[1] = 0x04;  // response accepted
            break;
        case AuthStage::Rejected:
            out[1] = 0x05;  // response rejected
            break;
        case AuthStage::NeedChallenge:
            return -1;  // host read before it sent the challenge request
    }
    std::memcpy(buf, out, sizeof(out));
    return static_cast<int>(sizeof(out));
}

std::string FakeTransport::Serial() const {
    return opened_ ? "FAKE0001" : "";
}
