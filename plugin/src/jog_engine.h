// jog_engine.h — wheel input → smooth playhead motion.
//
// This layer is the seam for the Speed Editor: IWheelSource is the only
// thing the engine cares about. Today it is DemoWheelSource (synthetic
// wheel turns). When the C++ driver from src/speed_editor/ is integrated,
// a HidWheelSource implementing the same interface wraps the hidapi poll
// loop and is handed to JogEngine instead — no other code changes.

#ifndef SPEED_REAPER_JOG_ENGINE_H
#define SPEED_REAPER_JOG_ENGINE_H

#include <cstdint>

// One relative wheel reading (signed ticks; the Speed Editor's jog report
// carries a signed 32-bit value, see docs/speed-editor-capabilities.md).
struct WheelSample {
  std::int32_t delta = 0; // relative delta since the last event; 0 = no motion
};

// Source of wheel events. Called once per engine tick from REAPER's timer
// (main thread). Must be non-blocking.
class IWheelSource {
 public:
  virtual ~IWheelSource() = default;
  virtual void Tick(WheelSample *out) = 0;
};

// Destination for smoothed motion. REAPER implements this (main.cpp) with
// CSurf_ScrubAmt — the same primitive the real editor will use.
class IPlayheadSink {
 public:
  virtual ~IPlayheadSink() = default;
  // Move the playhead/play cursor relatively (scrub). dt < 0 = backward.
  virtual void ScrubBySeconds(double dt) = 0;
  // Current play position, seconds (for logging/future LED feedback).
  virtual double PlayPosition() const = 0;
};

// Simulates one "turn" of the wheel: velocity ramps up, holds, decays to
// zero. Trigger() (re)starts a forward turn; Stop() halts immediately.
// Stands in for the hardware until the driver is integrated.
class DemoWheelSource : public IWheelSource {
 public:
  void Trigger(); // start (or restart) one wheel turn
  void Stop();
  bool Active() const { return t_ > 0.0; }

  void Tick(WheelSample *out) override;

  // Tuning (a real editor's absolute mode spans ±4096 per half turn).
  static constexpr double kRampUpS = 0.25;
  static constexpr double kHoldS = 0.50;
  static constexpr double kRampDownS = 0.50;
  static constexpr double kMaxDeltaPerTick = 53.0;
  static constexpr double kTickMs = 8.0; // must match the timer interval

 private:
  double t_ = 0.0; // seconds into the current turn; <= 0 = idle
};

// Converts raw wheel deltas into smooth relative playhead motion.
//
// Smoothing model: the engine tracks a velocity (wheel units per ms) and
// relaxes it toward the instantaneous wheel rate with an exponential
// low-pass (tau ≈ 50 ms). That is what makes the playhead glide instead of
// stepping — the same treatment the real editor's jittery relative report
// will need.
class JogEngine {
 public:
  JogEngine(IWheelSource &source, IPlayheadSink &sink) : source_(source), sink_(sink) {}

  // Advance one tick; dt_ms must match the REAPER timer interval.
  void Tick(double dt_ms);

  void ToggleDemo() {
    if (demo_.Active()) demo_.Stop();
    else demo_.Trigger();
    demo_on_ = demo_.Active();
  }
  void NudgeSeconds(double dt) { sink_.ScrubBySeconds(dt); }
  bool DemoActive() const { return demo_on_; }
  double Velocity() const { return vel_; } // wheel units per ms (debug)

  DemoWheelSource &Demo() { return demo_; }

  // Wheel unit → time mapping: 1 relative wheel tick = 0.1 ms of project
  // time (≈ one full turn at max speed moves ~0.5 s of timeline).
  static constexpr double kSecondsPerWheelUnit = 0.0001;

 private:
  IWheelSource &source_;
  IPlayheadSink &sink_;
  DemoWheelSource demo_;
  bool demo_on_ = false;
  double vel_ = 0.0;
};

#endif // SPEED_REAPER_JOG_ENGINE_H
