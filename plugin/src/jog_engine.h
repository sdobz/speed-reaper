// jog_engine.h — wheel input → playhead motion.
//
// This layer is the seam for the Speed Editor: IWheelSource is the only
// thing the engine cares about. The Session feeds it the WheelDispatcher
// (wheel_dispatcher.h), which reads the HidWheelSource and routes the delta
// (hold → gestures; else by mode) — no other code changes needed for new
// sources.

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

// Converts raw wheel deltas into playhead motion, 1:1: the playhead moves by
// exactly delta * kSecondsPerWheelUnit each tick, so it stops the moment the
// wheel does (no smoothing, no coasting, no queued backlog).
//
// Scale (measured on a real Speed Editor, docs/notes/m1-wheel-scale.md):
// one wheel revolution = ~1,474,560 units (4096 detents x 360). Mapping is
// "seconds of timeline per revolution"; candidate user parameter
// `jog.seconds_per_rev`.
class JogEngine {
 public:
  JogEngine(IWheelSource &source, IPlayheadSink &sink) : source_(source), sink_(sink) {}

  void Tick() {
    WheelSample ev;
    source_.Tick(&ev);
    if (ev.delta != 0) sink_.ScrubBySeconds(ev.delta * kSecondsPerWheelUnit);
  }

  static constexpr double kUnitsPerRev = 1474560.0; // 4096 * 360, measured
  static constexpr double kSecondsPerRev = 20.0;    // tunable: feel
  static constexpr double kSecondsPerWheelUnit = kSecondsPerRev / kUnitsPerRev;

 private:
  IWheelSource &source_;
  IPlayheadSink &sink_;
};

#endif // SPEED_REAPER_JOG_ENGINE_H
