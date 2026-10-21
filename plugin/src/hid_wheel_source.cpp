// hid_wheel_source.cpp — real worker-thread implementation (device
// open/read/write/close steps; the threading rule is in the header).
//
// Threading: the worker thread only talks to the driver (hid_device.h,
// which is included here only). It crosses data to the REAPER timer thread
// via the atomics and mutex queues declared in hid_wheel_source.h; the
// reverse direction is the PendingOutput struct. The driver is constructed
// with MakeTransport() (transport_factory.h) so test builds can select a
// scripted FakeTransport via SPEED_REAPER_FAKE_DEVICE.

#include "hid_wheel_source.h"

#include <chrono>
#include <cstdarg>
#include <utility>
#include <vector>

#include "speed_editor/hid_device.h"
#include "transport_factory.h"
#include "trace.h"

HidWheelSource::HidWheelSource() = default;

HidWheelSource::~HidWheelSource() { Stop(); }

void HidWheelSource::Start() {
  if (running_.load()) return;  // idempotent
  dev_ = std::make_unique<SpeedEditorHID>(MakeTransport());
  running_.store(true);
  worker_ = std::thread([this] { WorkerLoop(); });
}

void HidWheelSource::Stop() {
  if (!running_.exchange(false)) return;  // never started, or already stopped
  if (worker_.joinable()) worker_.join();
  // The worker closed the device on exit.
}

void HidWheelSource::WorkerLoop() {
  bool not_found_logged = false;  // `device not found` once until it appears
  while (running_.load()) {
    // Step 1: not open → retry open() every 1 s (prompt for Stop).
    while (running_.load() && !dev_->open("")) {
      if (!not_found_logged) {
        PushLog("device not found");
        not_found_logged = true;
      }
      SleepSliced(1000);
    }
    if (!running_.load()) break;
    not_found_logged = false;

    // Step 2: authenticate; on failure back to 1.
    if (!dev_->authenticate()) {
      PushLog("auth failed");
      dev_->close();
      continue;
    }

    // Step 3: init and announce.
    dev_->setWheelMode(0);
    dev_->setJogLeds(true, false, false);
    connected_.store(true);
    PushLog("connected (serial " + dev_->getSerial() + ")");

    // Step 4: event loop.
    std::vector<std::uint16_t> held;  // previously held key set
    while (running_.load()) {
      ApplyPending();
      SpeedEditorEvent ev;
      if (!dev_->readEvent(ev, 20)) {
        // false = timeout OR disconnect; distinguish via driver state.
        if (dev_->getState() == SpeedEditorHID::State::Disconnected) {
          connected_.store(false);
          held.clear();
          PushLog("disconnected");
          break;  // back to step 1
        }
        continue;  // timeout
      }
      switch (ev.type) {
        case EventType::Wheel: {
          if (ev.wheel.mode == 0 || ev.wheel.mode == 2)
            delta_.fetch_add(ev.wheel.delta);
          else  // modes 1/3: absolute position
            abs_.store(ev.wheel.delta);
          break;
        }
        case EventType::Button: {
          std::vector<ButtonChange> changes;
          DiffHeldKeys(held, ev.button.keycodes, &changes);
          held = ev.button.keycodes;
          if (!changes.empty()) {
            std::lock_guard<std::mutex> lk(button_mutex_);
            for (const ButtonChange &c : changes) buttons_.push_back(c);
          }
          break;
        }
        default:
          break;  // battery / unknown: ignored
      }
    }
  }

  // Step 5: Stop() → close (clears LEDs).
  dev_->close();
}

void HidWheelSource::PushLog(const std::string &line) {
  {
    std::lock_guard<std::mutex> lk(log_mutex_);
    log_.push_back(line);
  }
  Trace("log %s", line.c_str());
}

void HidWheelSource::ApplyPending() {
  PendingOutput p;
  {
    std::lock_guard<std::mutex> lk(pending_mutex_);
    p = pending_;
    pending_.wheel_mode_dirty = false;
    pending_.jog_leds_dirty = false;
    pending_.leds_dirty = false;
  }
  if (p.wheel_mode_dirty) dev_->setWheelMode(p.wheel_mode);
  if (p.jog_leds_dirty) dev_->setJogLeds(p.jog, p.shtl, p.scrl);
  if (p.leds_dirty) {
    // 18 main-panel LEDs → LE32 bitfield, report 0x02 (LedsReportBytes).
    const auto b = LedsReportBytes(p.leds);
    dev_->setLeds(b[0], b[1], b[2]);
  }
}

bool HidWheelSource::SleepSliced(int ms) {
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (!running_.load()) return false;
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now());
    std::this_thread::sleep_for(std::min(left, std::chrono::milliseconds(100)));
  }
  return true;
}

void HidWheelSource::Tick(WheelSample *out) { out->delta = delta_.exchange(0); }

bool HidWheelSource::PopButton(ButtonChange *out) {
  std::lock_guard<std::mutex> lk(button_mutex_);
  if (buttons_.empty()) return false;
  *out = buttons_.front();
  buttons_.pop_front();
  return true;
}

bool HidWheelSource::PopLog(std::string *out) {
  std::lock_guard<std::mutex> lk(log_mutex_);
  if (log_.empty()) return false;
  *out = std::move(log_.front());
  log_.pop_front();
  return true;
}

bool HidWheelSource::Connected() const {
  return connected_.load();
}

std::int32_t HidWheelSource::AbsolutePosition() const {
  return abs_.load();
}

void HidWheelSource::RequestWheelMode(std::uint8_t protocolMode) {
  std::lock_guard<std::mutex> lk(pending_mutex_);
  pending_.wheel_mode_dirty = true;
  pending_.wheel_mode = protocolMode;
}

void HidWheelSource::RequestJogLeds(bool jog, bool shtl, bool scrl) {
  std::lock_guard<std::mutex> lk(pending_mutex_);
  pending_.jog_leds_dirty = true;
  pending_.jog = jog;
  pending_.shtl = shtl;
  pending_.scrl = scrl;
}

void HidWheelSource::RequestLeds(std::uint32_t mainMask) {
  std::lock_guard<std::mutex> lk(pending_mutex_);
  pending_.leds_dirty = true;
  pending_.leds = mainMask;
}
