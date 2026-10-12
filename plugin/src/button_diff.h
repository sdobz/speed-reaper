// button_diff.h — pure held-key-set diff (docs/plugin-design-m1-m3.md §6).
// REAPER-free and HID-free: unit-testable in isolation.
//
// The worker thread calls DiffHeldKeys for every button event (design §4
// step 4): `prev` = keycodes held before the event, `cur` = keycodes held
// after it. Output order: releases first (in `prev` order), then presses
// (in `cur` order), so a key released and re-pressed between two events
// is reported as a release before any new press.

#ifndef SPEED_REAPER_BUTTON_DIFF_H
#define SPEED_REAPER_BUTTON_DIFF_H

#include <cstdint>
#include <vector>

// One button edge, worker → main (design §2). key = KEY_* from key_map.h.
struct ButtonChange {
  std::uint16_t key;
  bool down;
};

// Diff two held key sets into button edges appended to *out.
// Releases come first (in `prev` order), then presses (in `cur` order).
void DiffHeldKeys(const std::vector<std::uint16_t> &prev,
                  const std::vector<std::uint16_t> &cur,
                  std::vector<ButtonChange> *out);

#endif  // SPEED_REAPER_BUTTON_DIFF_H
