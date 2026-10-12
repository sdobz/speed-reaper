// jog_engine.h — wheel input → playhead motion.
//
// This layer is the seam for the Speed Editor: IWheelSource is the only
// thing the engine cares about. Today main.cpp feeds it a
// SummedWheelSource(DemoWheelSource, NullWheelSource). When the C++ driver
// from src/speed_editor/ is integrated,
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

// Always reports no motion. Lets the engine be built from a source that
// can be swapped in/out without rebuilding (main.cpp: today it is summed
// with the demo source; task 6 replaces it with the HID source).
struct NullWheelSource : IWheelSource {
  void Tick(WheelSample *out) override { out->delta = 0; }
};

// Sums two sources: each tick, both are ticked and their deltas added.
class SummedWheelSource : public IWheelSource {
 public:
  SummedWheelSource(IWheelSource &a, IWheelSource &b) : a_(a), b_(b) {}
  void Tick(WheelSample *out) override {
    WheelSample sa, sb;
    a_.Tick(&sa);
    b_.Tick(&sb);
    out->delta = sa.delta + sb.delta;
  }

 private:
  IWheelSource &a_;
  IWheelSource &b_;
};

// Destination for wheel motion. REAPER implements this (main.cpp) with
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

  // Tuning (units per tick at 125 Hz).
  static constexpr double kRampUpS = 0.25;
  static constexpr double kHoldS = 0.50;
  static constexpr double kRampDownS = 0.50;
  static constexpr double kMaxDeltaPerTick = 390.0; // ≈0.66 s of timeline per s at the hold; demo total ≈0.58 s
  static constexpr double kTickMs = 8.0; // must match the timer interval

 private:
  double t_ = 0.0; // seconds into the current turn; <= 0 = idle
};

// Converts raw wheel deltas into playhead motion, 1:1: the playhead moves by
// exactly delta * kSecondsPerWheelUnit each tick, so it stops the moment the
// wheel does (no smoothing, no coasting, no queued backlog).
//
// Scale (measured on a real Speed Editor, docs/notes/m1-wheel-scale.md):
// one wheel revolution = ~1,474,560 units (4096 detents x 360). Mapping is
// "seconds of timeline per revolution"; candidate user parameter
// `jog.seconds_per_rev` (docs/plugin-configuration.md §2.2).
class JogEngine {
 public:
  JogEngine(IWheelSource &source, IPlayheadSink &sink) : source_(source), sink_(sink) {}

  void Tick() {
    WheelSample ev;
    source_.Tick(&ev);
    if (ev.delta != 0) sink_.ScrubBySeconds(ev.delta * kSecondsPerWheelUnit);
  }

  void NudgeSeconds(double dt) { sink_.ScrubBySeconds(dt); }

  static constexpr double kUnitsPerRev = 1474560.0; // 4096 * 360, measured
  static constexpr double kSecondsPerRev = 20.0;    // tunable: feel
  static constexpr double kSecondsPerWheelUnit = kSecondsPerRev / kUnitsPerRev;

 private:
  IWheelSource &source_;
  IPlayheadSink &sink_;
};

#endif // SPEED_REAPER_JOG_ENGINE_H
