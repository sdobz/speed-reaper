#include "gesture.h"

#include <algorithm>

void GestureEngine::SetDoubleTapKeys(std::vector<std::uint16_t> keys) {
  double_tap_keys_ = std::move(keys);
}

bool GestureEngine::IsDoubleTapKey(std::uint16_t key) const {
  return std::find(double_tap_keys_.begin(), double_tap_keys_.end(), key) !=
         double_tap_keys_.end();
}

void GestureEngine::OnKey(std::uint16_t key, bool down, std::int64_t now_ms) {
  KeyState &st = keys_[key];
  if (down) {
    if (st.waiting_second) {
      // Second press inside the double-tap window: one DoubleTap, and the
      // release of this press must not also emit a Tap.
      st.waiting_second = false;
      st.suppress_release_tap = true;
      Emit(Gesture{key, GestureKind::DoubleTap, 0});
    } else {
      st.suppress_release_tap = false;  // clean press
    }
    st.pressed = true;
    st.hold_started = false;
    st.down_ms = now_ms;
    return;
  }
  if (!st.pressed) return;
  st.pressed = false;
  if (st.hold_started) {
    st.hold_started = false;
    Emit(Gesture{key, GestureKind::HoldEnd, 0});  // hold release: no Tap
  } else if (st.suppress_release_tap) {
    st.suppress_release_tap = false;
  } else if (IsDoubleTapKey(key)) {
    // Defer the tap; a second press inside double_tap_ms turns it into a
    // DoubleTap, Poll promotes it to a Tap once the window expires.
    st.waiting_second = true;
    st.release_ms = now_ms;
  } else {
    Emit(Gesture{key, GestureKind::Tap, 0});
  }
}

void GestureEngine::OnWheel(std::int32_t delta, std::int64_t now_ms) {
  (void)now_ms;
  if (delta == 0) return;
  // Wheel motion while held applies to the primary held key (the earliest
  // press); wheel motion with no key held is a no-op here (the caller keeps
  // it on the jog path).
  bool found = false;
  std::uint16_t key = 0;
  std::int64_t down_ms = 0;
  for (const auto &kv : keys_) {
    const KeyState &st = kv.second;
    if (!st.pressed) continue;
    if (!found || st.down_ms < down_ms) {
      found = true;
      key = kv.first;
      down_ms = st.down_ms;
    }
  }
  if (!found) return;
  KeyState &st = keys_[key];
  if (!st.hold_started) {
    st.hold_started = true;
    Emit(Gesture{key, GestureKind::HoldStart, 0});
  }
  Emit(Gesture{key, GestureKind::HoldRotate, delta});
}

void GestureEngine::Poll(std::int64_t now_ms) {
  for (auto &kv : keys_) {
    KeyState &st = kv.second;
    if (st.pressed && !st.hold_started &&
        now_ms - st.down_ms >= cfg_.hold_ms) {
      st.hold_started = true;
      Emit(Gesture{kv.first, GestureKind::HoldStart, 0});
    } else if (st.waiting_second &&
               now_ms - st.release_ms >= cfg_.double_tap_ms) {
      st.waiting_second = false;
      Emit(Gesture{kv.first, GestureKind::Tap, 0});  // window expired: one Tap
    }
  }
}

bool GestureEngine::Pop(Gesture *out) {
  if (pending_.empty()) return false;
  *out = pending_.front();
  pending_.erase(pending_.begin());
  return true;
}

bool GestureEngine::HoldActive() const {
  for (const auto &kv : keys_)
    if (kv.second.pressed) return true;
  return false;
}
