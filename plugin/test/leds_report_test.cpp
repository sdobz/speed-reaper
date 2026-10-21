// LedsReportBytes (src/speed_editor/hid_device.h): the 18-LED main-panel
// mask is the LE32 bitfield of output report 0x02 — bit i lives in byte
// i/8 at position i%8 (b0 = bits 0–7, b1 = bits 8–15, b2 = bits 16–17).
// Regression: hid_wheel_source.cpp once packed the mask as three 6-bit
// groups, which on real hardware lit the wrong physical LEDs (plugin bit i
// landed on device bit (i/6)*8 + i%6 — e.g. bit 8 lit CAM4 instead of
// CAM9; bits 14–17 lit nothing). Field evidence: vibes/45-cam-n.md,
// confirmed by the speedcli LED chase demo matching key_map.h.
#include "check.h"
#include "speed_editor/hid_device.h"

#include <array>
#include <cstdint>

namespace {

void everyBit() {
  for (int i = 0; i < 18; ++i) {
    const auto b = LedsReportBytes(std::uint32_t{1} << i);
    const int byte = i / 8, pos = i % 8;
    for (int y = 0; y < 3; ++y)
      CHECK(b[y] ==
            (y == byte ? static_cast<std::uint8_t>(1u << pos) : 0u));
  }
  // Bits beyond 17 are dropped (the device defines no more LEDs).
  const auto b18 = LedsReportBytes(std::uint32_t{1} << 18);
  CHECK(b18 == (std::array<std::uint8_t, 3>{0x00, 0x00, 0x00}));
  const auto b31 = LedsReportBytes(0x80000000u);
  CHECK(b31 == (std::array<std::uint8_t, 3>{0x00, 0x00, 0x00}));
}

void regressionShapes() {
  // The exact bits the field test mislit, with the byte split that fixed
  // them (old 6-bit split in comments for contrast):
  const auto b6 = LedsReportBytes(1u << 6);   // was (00 01 00) -> device bit 8 = CAM9
  CHECK(b6 == (std::array<std::uint8_t, 3>{0x40, 0x00, 0x00}));
  const auto b8 = LedsReportBytes(1u << 8);   // was (00 04 00) -> device bit 10 = CAM4
  CHECK(b8 == (std::array<std::uint8_t, 3>{0x00, 0x01, 0x00}));
  const auto b12 = LedsReportBytes(1u << 12);  // was (00 00 01) -> device bit 16 = CAM3
  CHECK(b12 == (std::array<std::uint8_t, 3>{0x00, 0x10, 0x00}));
  const auto b16 = LedsReportBytes(1u << 16);  // was (00 00 04) -> device bit 20 = none
  CHECK(b16 == (std::array<std::uint8_t, 3>{0x00, 0x00, 0x01}));
}

void fullMask() {
  // All 18 LEDs: 0x3FFFF -> FF FF 03 (byte 2 holds CAM3 + AUDIO_ONLY only).
  CHECK(LedsReportBytes(0x3FFFFu) ==
        (std::array<std::uint8_t, 3>{0xFF, 0xFF, 0x03}));
  CHECK(LedsReportBytes(0u) ==
        (std::array<std::uint8_t, 3>{0x00, 0x00, 0x00}));
}

}  // namespace

int main() {
  everyBit();
  regressionShapes();
  fullMask();
  return check_failures();
}
