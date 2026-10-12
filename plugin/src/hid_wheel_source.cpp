// hid_wheel_source.cpp — STUB (vibes/20-m1-wheel-playhead.md task 3).
//
// The public interface is already final per docs/plugin-design-m1-m3.md §4;
// task 5 replaces this body with the worker loop (design §4 steps 1–5 and
// §2: atomic delta accumulator, mutex queues, fixed log strings). Until
// then: Start/Stop are no-ops, Pop* return false, Tick reports no motion.

#include "hid_wheel_source.h"

#include "speed_editor/hid_device.h"

HidWheelSource::HidWheelSource() = default;

HidWheelSource::~HidWheelSource() { Stop(); }

void HidWheelSource::Start() {
  // Stub: no worker thread yet (task 5).
}

void HidWheelSource::Stop() {
  // Stub: nothing to join, no device to close (task 5).
}

void HidWheelSource::Tick(WheelSample *out) {
  out->delta = 0;  // stub: no wheel motion
}

bool HidWheelSource::PopButton(ButtonChange *out) {
  (void)out;
  return false;  // stub: no buttons
}

bool HidWheelSource::PopLog(std::string *out) {
  (void)out;
  return false;  // stub: no log lines
}

bool HidWheelSource::Connected() const {
  return connected_.load();  // false until task 5
}

std::int32_t HidWheelSource::AbsolutePosition() const {
  return abs_.load();  // 0 until task 5
}

void HidWheelSource::RequestWheelMode(std::uint8_t protocolMode) {
  (void)protocolMode;  // stub: no worker to apply it
}

void HidWheelSource::RequestJogLeds(bool jog, bool shtl, bool scrl) {
  (void)jog;
  (void)shtl;
  (void)scrl;  // stub
}

void HidWheelSource::RequestLeds(std::uint32_t mainMask) {
  (void)mainMask;  // stub
}
