// Characterizes DiffHeldKeys (docs/plugin-design-m1-m3.md §6): releases
// first (in prev order), then presses (in cur order). No REAPER needed.
#include "button_diff.h"
#include "check.h"

namespace {
std::uint16_t A = 0x01, B = 0x02, C = 0x03;

void Expect(const std::vector<ButtonChange> &got,
            const std::vector<ButtonChange> &want) {
  CHECK(got.size() == want.size());
  for (std::size_t i = 0; got.size() == want.size() && i < got.size(); ++i)
    CHECK(got[i].key == want[i].key && got[i].down == want[i].down);
}

void ExpectNone(std::vector<ButtonChange> *out, const std::vector<std::uint16_t> &prev,
                const std::vector<std::uint16_t> &cur) {
  DiffHeldKeys(prev, cur, out);
  CHECK(out->empty());
}
}  // namespace

int main() {
  // Press: empty prev, one key held now.
  {
    std::vector<ButtonChange> out;
    DiffHeldKeys({}, {A}, &out);
    Expect(out, {{A, true}});
  }
  // Release: one key held before, none now.
  {
    std::vector<ButtonChange> out;
    DiffHeldKeys({A}, {}, &out);
    Expect(out, {{A, false}});
  }
  // Two keys held, one released: only the release, no re-press.
  {
    std::vector<ButtonChange> out;
    DiffHeldKeys({A, B}, {B}, &out);
    Expect(out, {{A, false}});
  }
  // Release-all (empty cur): releases in prev order.
  {
    std::vector<ButtonChange> out;
    DiffHeldKeys({A, B}, {}, &out);
    Expect(out, {{A, false}, {B, false}});
  }
  // No change → no output.
  {
    std::vector<ButtonChange> out;
    ExpectNone(&out, {A, B}, {A, B});
    ExpectNone(&out, {}, {});
  }
  // Ordering: releases first (in prev order), then presses (in cur order).
  {
    std::vector<ButtonChange> out;
    DiffHeldKeys({A, B}, {C, B}, &out);
    Expect(out, {{A, false}, {C, true}});
  }
  // Reorder of the held set alone is not an edge.
  {
    std::vector<ButtonChange> out;
    ExpectNone(&out, {A, B}, {B, A});
  }
  return check_failures();
}
