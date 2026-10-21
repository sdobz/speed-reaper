// default_bindings_test.cpp — golden test (41-action-registry §3): the
// embedded default bindings reproduce today's behavior exactly. Parses the
// build-time-embedded default_bindings.ini (generated header) with zero
// errors; the derived double-tap set equals today's the former double-tap set
// (IN, OUT, ESC, TIMELINE, AUDIO_LEVEL); and every (key, gesture) today's
// tables (the old id table, the old composite rows, the old hold rows, Router press rules) map is
// bound to the matching verb — with no extras.
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "bindings.h"
#include "check.h"
#include "default_bindings_generated.h"
#include "speed_editor/key_map.h"
#include "verb_registry.h"

namespace {

struct Expected {
  std::uint16_t key;
  BindingGesture gesture;
  std::string name;
  std::vector<std::string> args;
};

// Every (key, gesture) today's tables map, with the verb + args the
// default bindings must bind it to:
//  - Router press rules (router.cpp OnButton): JOG/SCRL/SHTL/STOP_PLAY
//  - the old id table (verbs.cpp): (key, tap/double) → command id
//  - the old composite rows (verbs.cpp): the function-verb rows
//  - the old hold rows (verbs.cpp): hold + wheel nudge modes
const std::vector<Expected> kExpected = {
    // Router press rules
    {KEY_JOG, BindingGesture::Press, "wheel", {"jog"}},
    {KEY_SCRL, BindingGesture::Press, "wheel", {"scrl"}},
    {KEY_SHTL, BindingGesture::Press, "wheel", {"shtl"}},
    {KEY_STOP_PLAY, BindingGesture::Press, "transport", {"play_stop"}},
    // the old id table
    {KEY_SPLIT, BindingGesture::Tap, "action", {"40012"}},
    {KEY_IN, BindingGesture::Tap, "action", {"40625"}},
    {KEY_OUT, BindingGesture::Tap, "action", {"40626"}},
    {KEY_IN, BindingGesture::Double, "action", {"40635"}},
    {KEY_OUT, BindingGesture::Double, "action", {"40635"}},
    {KEY_SNAP, BindingGesture::Tap, "action", {"1157"}},
    {KEY_ESC, BindingGesture::Tap, "action", {"40635"}},
    {KEY_ESC, BindingGesture::Double, "action", {"40029"}},
    {KEY_AUDIO_LEVEL, BindingGesture::Double, "action", {"40157"}},
    {KEY_FULL_VIEW, BindingGesture::Tap, "action", {"40346"}},
    // the old composite rows
    {KEY_TRIM_IN, BindingGesture::Tap, "targeted", {"40511"}},
    {KEY_TRIM_OUT, BindingGesture::Tap, "targeted", {"40512"}},
    {KEY_RIPL_DEL, BindingGesture::Tap, "ripple_delete", {}},
    {KEY_TIMELINE, BindingGesture::Tap, "select_cursor_item", {}},
    {KEY_TIMELINE, BindingGesture::Double, "action", {"40289"}},
    {KEY_CAM1, BindingGesture::Tap, "take", {"1"}},
    {KEY_CAM2, BindingGesture::Tap, "take", {"2"}},
    {KEY_CAM3, BindingGesture::Tap, "take", {"3"}},
    {KEY_CAM4, BindingGesture::Tap, "take", {"4"}},
    {KEY_CAM5, BindingGesture::Tap, "take", {"5"}},
    {KEY_CAM6, BindingGesture::Tap, "take", {"6"}},
    {KEY_CAM7, BindingGesture::Tap, "take", {"7"}},
    {KEY_CAM8, BindingGesture::Tap, "take", {"8"}},
    {KEY_CAM9, BindingGesture::Tap, "take", {"9"}},
    {KEY_AUDIO_LEVEL, BindingGesture::Tap, "volume_readout", {}},
    {KEY_SMTH_CUT, BindingGesture::Tap, "smooth_cut", {}},
    {KEY_PLACE_ON_TOP, BindingGesture::Tap, "place_on_top", {}},
    {KEY_VIDEO_ONLY, BindingGesture::Tap, "isolate_tracks", {}},
    {KEY_AUDIO_ONLY, BindingGesture::Tap, "solo_tracks", {}},
    // the old hold rows (the turn verbs)
    {KEY_SPLIT, BindingGesture::Turn, "nudge", {"move"}},
    {KEY_TRIM_IN, BindingGesture::Turn, "nudge", {"trim_in"}},
    {KEY_TRIM_OUT, BindingGesture::Turn, "nudge", {"trim_out"}},
    {KEY_SLIP_SRC, BindingGesture::Turn, "nudge", {"slip_src"}},
    {KEY_SLIP_DEST, BindingGesture::Turn, "nudge", {"slip_dest"}},
    {KEY_AUDIO_LEVEL, BindingGesture::Turn, "nudge", {"volume"}},
    // the last CompositeRow: TIMELINE hold + rotate
    {KEY_TIMELINE, BindingGesture::Turn, "select_step", {}},
    {KEY_IN,       BindingGesture::Turn, "nudge", {"sel_in"}},
    {KEY_OUT,      BindingGesture::Turn, "nudge", {"sel_out"}},
    {KEY_ROLL,     BindingGesture::Turn, "nudge", {"roll"}},
    // crossfades (docs/notes/crossfade-actions.md)
    {KEY_TRANS,     BindingGesture::Tap, "action", {"41059"}},
    {KEY_DIS,       BindingGesture::Tap, "action", {"40311"}},
    {KEY_CUT,       BindingGesture::Tap, "action", {"40310"}},
    {KEY_LIVE_OWR,  BindingGesture::Tap, "action", {"41117"}},
    {KEY_TRANS_DUR, BindingGesture::Tap, "action", {"43776"}},
};

}  // namespace

int main() {
  const Bindings b = ParseBindings(kDefaultBindingsIni, MakeDefaultRegistry());

  // 1) The embedded file parses with zero errors.
  CHECK(b.errors.empty());

  // 2) The derived double-tap set (sorted by key code) equals today's
  // the former double-tap set: {IN, OUT, ESC, TIMELINE, AUDIO_LEVEL}.
  CHECK((b.double_tap_keys ==
         std::vector<std::uint16_t>{KEY_IN, KEY_OUT, KEY_TIMELINE,
                                    KEY_AUDIO_LEVEL, KEY_ESC}));

  // 3) Every (key, gesture) today's tables map is bound to the matching
  // verb — and the binding set is exactly that set (no extras, no
  // duplicates): one entry per expected row, nothing else.
  CHECK(b.bindings.size() == kExpected.size());
  for (const Expected &e : kExpected) {
    const auto it = b.bindings.find({e.key, e.gesture});
    if (it == b.bindings.end()) {
      std::fprintf(stderr, "%s:%d: missing binding for key 0x%04X: expected "
                           "verb '%s'\n",
                   __FILE__, __LINE__, e.key, e.name.c_str());
      ++check_failures();
      continue;
    }
    CHECK(it->second.name == e.name);
    CHECK((it->second.args == e.args));
    CHECK(it->second.verb != nullptr);
  }

  // 4) Keys whose label has no clear REAPER meaning ship unbound (any
  // gesture); suggestions live in comments only.
  for (const std::uint16_t key : {KEY_SMART_INSRT, KEY_APPND, KEY_RIPL_OWR,
                                  KEY_SRC_OWR, KEY_CLOSE_UP, KEY_SYNC_BIN,
                                  KEY_SOURCE}) {
    for (const BindingGesture g : {BindingGesture::Press, BindingGesture::Tap,
                                   BindingGesture::Double, BindingGesture::Turn})
      CHECK(b.bindings.find({key, g}) == b.bindings.end());
  }

  return check_failures();
}
