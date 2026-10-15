// bindings.h — parse bindings.ini into tuning + (key, gesture) -> verb.
//
// Pure: no REAPER API, no filesystem — the caller reads/writes the file
// and passes the text. Layout (41 design):
//
//   [tuning]                double_tap_ms / hold_ms / hold_ticks_per_step
//   [bindings]              KEY.gesture = verb [args]     ; comment
//
// `;`/`#` start a comment anywhere on a line; blank lines are skipped;
// whitespace around keys, values, and sections is tolerated; an unknown
// section is an error line. Errors never stop parsing: each bad line
// yields one "bindings.ini:<n>: <reason>" string and is skipped.
// A key bound to `press` must not have any other gesture.
//
// Verbs are built against the given registry at parse time, so the
// registry must outlive the result (it owns the WheelState the
// wheel/transport verbs capture).

#ifndef SPEED_REAPER_BINDINGS_H
#define SPEED_REAPER_BINDINGS_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "verb_registry.h"

// The [tuning] section (defaults match GestureConfig in gesture.h;
// hold_ticks_per_step is the turn-verb step granularity, 41 design).
struct Tuning {
  int double_tap_ms = 300;
  int hold_ms = 350;
  int hold_ticks_per_step = 4;
};

// One binding: the verb name + args exactly as written, plus the verb
// built from the registry.
struct BoundVerb {
  std::string name;
  std::vector<std::string> args;
  std::unique_ptr<Verb> verb;
  // Non-copyable (owns the verb); still insertable into std::map.
  BoundVerb() = default;
  BoundVerb(BoundVerb &&) = default;
  BoundVerb &operator=(BoundVerb &&) = default;
  BoundVerb(const BoundVerb &) = delete;
  BoundVerb &operator=(const BoundVerb &) = delete;
};

struct Bindings {
  Tuning tuning;
  std::map<std::pair<std::uint16_t, BindingGesture>, BoundVerb> bindings;
  // One entry per bad line: "bindings.ini:<line>: <reason>" (1-based).
  std::vector<std::string> errors;
  // Keys with a `.double` binding: only those taps wait for a second
  // press (the double-tap set is derived, not listed). Sorted.
  std::vector<std::uint16_t> double_tap_keys;
};

// Parse the whole file. Verb resolution (unknown name, wrong gesture,
// bad args) goes through the registry; the registry's error text is
// surfaced verbatim as the line reason.
Bindings ParseBindings(std::string_view text, const VerbRegistry &registry);

// "SPLIT" -> 0x002F: the KEY_* suffix names from src/speed_editor/key_map.h
// (STOP/PLAY reads STOP_PLAY, RIPL O/WR reads RIPL_OWR, ...). Unknown
// names return 0; KeyName(0) is "".
std::uint16_t KeyByName(const std::string &name);
std::string KeyName(std::uint16_t key);
// Every nameable key code, in key_map order (for the show-bindings list).
std::vector<std::uint16_t> AllKeys();

// Pure INI rewrite for the learn action: set `<lhs> = <rhs>` (lhs e.g.
// "TRIM_IN.tap", rhs e.g. "action 40012") in the [bindings] section. An
// existing line for the same lhs (whitespace-insensitive, a trailing
// comment is dropped) is replaced in place; otherwise the line is appended
// at the end of [bindings] (the section is created if missing). Every
// other line, comments included, is preserved byte for byte.
std::string SetBindingLine(std::string_view ini, std::string_view lhs,
                           std::string_view rhs);

#endif  // SPEED_REAPER_BINDINGS_H
