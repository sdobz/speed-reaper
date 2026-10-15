// button_diff.cpp — pure held-key-set diff.
#include "button_diff.h"

#include <algorithm>

void DiffHeldKeys(const std::vector<std::uint16_t> &prev,
                  const std::vector<std::uint16_t> &cur,
                  std::vector<ButtonChange> *out) {
  for (std::uint16_t key : prev)
    if (std::find(cur.begin(), cur.end(), key) == cur.end())
      out->push_back({key, false});  // release, in `prev` order
  for (std::uint16_t key : cur)
    if (std::find(prev.begin(), prev.end(), key) == prev.end())
      out->push_back({key, true});  // press, in `cur` order
}
