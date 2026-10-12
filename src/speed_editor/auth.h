#pragma once
// Speed Editor auth handshake math (report 0x06).
// Algorithm from smunaut's reverse-engineering, as ported from
// docs/speed-editor-examples/cpp-driver/src/hid_device.cpp (BSD-3-Clause).
// Extracted to free functions so both sides of the handshake can use it:
// the host (SpeedEditorHID::authenticate) and the fake device
// (FakeTransport, src/speed_editor/fake_transport.cpp). No behavior change.
#include <cstdint>

namespace speed_editor_auth {

// ── Authentication tables (from smunaut's reverse-engineering) ──────

inline const uint64_t (&authEven())[8] {
    static const uint64_t k[8] = {
        0x3AE1206F97C10BC8ULL, 0x2A9AB32BEBF244C6ULL,
        0x20A6F8B8DF9ADF0AULL, 0xAF80ECE52CFC1719ULL,
        0xEC2EE2F7414FD151ULL, 0xB055ADFD73344A15ULL,
        0xA63D2E3059001187ULL, 0x751BF623F42E0DDEULL,
    };
    return k;
}

inline const uint64_t (&authOdd())[8] {
    static const uint64_t k[8] = {
        0x3E22B34F502E7FDEULL, 0x24656B981875AB1CULL,
        0xA17F3456DF7BF8C3ULL, 0x6DF72E1941AEF698ULL,
        0x72226F011E66AB94ULL, 0x3831A3C606296B42ULL,
        0xFD7FF81881332C89ULL, 0x61A3F6474FF236C6ULL,
    };
    return k;
}

inline const uint64_t AUTH_MASK = 0xA79A63F585D37BF0ULL;

// Rotate left by one byte.
inline uint64_t rol8(uint64_t value) {
    return ((value << 56) | (value >> 8)) & 0xFFFFFFFFFFFFFFFFULL;
}

inline uint64_t rol8n(uint64_t value, int n) {
    for (int i = 0; i < n; i++) {
        value = rol8(value);
    }
    return value;
}

// Expected host response to a device challenge.
inline uint64_t calculateResponse(uint64_t challenge) {
    int n = challenge & 7;
    uint64_t v = rol8n(challenge, n);
    uint64_t k;

    if ((v & 1) == ((0x78 >> n) & 1)) {
        k = authEven()[n];
    }
    else {
        v ^= rol8(v);
        k = authOdd()[n];
    }

    return v ^ (rol8(v) & AUTH_MASK) ^ k;
}

}  // namespace speed_editor_auth
