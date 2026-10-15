// hid_wheel_source.h — real Speed Editor wheel source. Wraps the hidraw
// driver (src/speed_editor/hid_device.h, "SpeedEditorHID") in a worker
// thread and adapts it to IWheelSource so the existing
// IWheelSource → JogEngine → IPlayheadSink pipeline needs no changes.
//
// Threading: only the REAPER timer callback (main thread) may touch
// REAPER; the worker thread only talks to hidapi. Cross-thread data
// crosses via atomics and mutex-protected queues declared below.
//
// The first implementation was a stub (hid_wheel_source.cpp): correct
// interface, no worker thread; the worker loop was filled in later.

#ifndef SPEED_REAPER_HID_WHEEL_SOURCE_H
#define SPEED_REAPER_HID_WHEEL_SOURCE_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "button_diff.h"
#include "session.h"  // IDevice: this class IS the Session's real device

// The driver (src/speed_editor/hid_device.h) is included in hid_wheel_source.cpp
// only; forward-declare so the header stays REAPER- and hidapi-header-free.
class SpeedEditorHID;

class HidWheelSource : public IDevice {
 public:
  HidWheelSource();
  ~HidWheelSource() override;  // calls Stop()
  void Start();                // spawn worker; idempotent
  void Stop();                 // set flag, join; idempotent; closes device (LEDs off)
  void Tick(WheelSample *out) override;  // main thread; delta = accumulated since last Tick
  bool PopButton(ButtonChange *out);     // main thread; false if empty
  bool PopLog(std::string *out);         // main thread; false if empty
  bool Connected() const;                // atomic
  std::int32_t AbsolutePosition() const;  // latest mode 1/3 reading
  void RequestWheelMode(std::uint8_t protocolMode);
  void RequestJogLeds(bool jog, bool shtl, bool scrl);
  void RequestLeds(std::uint32_t mainMask);  // bit i = LED bit i of key_map.h

 private:
  // Main → worker pending outputs: request* setters mark dirty
  // under pending_mutex_; the worker applies them between reads.
  struct PendingOutput {
    bool wheel_mode_dirty = false;
    std::uint8_t wheel_mode = 0;  // 0-3 protocol mode
    bool jog_leds_dirty = false;
    bool jog = false;
    bool shtl = false;
    bool scrl = false;
    bool leds_dirty = false;
    std::uint32_t leds = 0;  // bit i = LED bit i of key_map.h
  };

  std::unique_ptr<SpeedEditorHID> dev_;  // driver (hid_device.h in .cpp only)
  std::thread worker_;
  std::atomic<bool> running_{false};

  std::atomic<std::int32_t> delta_{0};    // worker → main: relative accumulator
  std::atomic<std::int32_t> abs_{0};      // worker → main: latest absolute reading
  std::atomic<bool> connected_{false};

  std::mutex log_mutex_;
  std::deque<std::string> log_;

  std::mutex button_mutex_;
  std::deque<ButtonChange> buttons_;

  std::mutex pending_mutex_;
  PendingOutput pending_;

  // Worker-thread-only helpers (device open/read/write/close steps).
  void WorkerLoop();
  void PushLog(const std::string &line);  // enqueue + Trace
  void ApplyPending();                    // apply + clear dirty flags
  bool SleepSliced(int ms);               // sleep ≤100 ms slices; false if stopped
};

#endif  // SPEED_REAPER_HID_WHEEL_SOURCE_H
