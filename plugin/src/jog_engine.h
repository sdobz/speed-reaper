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

// Source of the live zoom scale: timeline pixels per second at the
// current arrange zoom (docs/jog-discovery.md §4.1, option a). Keeps the
// engine pure (REAPER-free), matching the IWheelSource / IPlayheadSink
// shape; the REAPER implementation is ReaperZoomSource (reaper_sinks.cpp)
// over GetHZoomLevel. Reading the live pxPerSec (rather than deriving it
// from the window width) is what makes the motion invariant across zoom
// and window resizes.
class IZoomSource {
 public:
  virtual ~IZoomSource() = default;
  // Timeline pixels per second at the current zoom.
  virtual double PixelsPerSecond() const = 0;
};

// Converts raw wheel deltas into playhead motion, 1:1: the playhead moves
// exactly this tick's delta's share, so it stops the moment the wheel does
// (no smoothing, no coasting, no queued backlog).
//
// Scale (docs/jog-discovery.md §4.1): screen-space-constant — one detent
// moves `pixels_per_detent` (k_px, `wheel.pixels_per_detent`) screen
// pixels at any zoom: per tick
//   dt = (delta / 360) * (k_px / pxPerSec)
// so dt · pxPerSec = (delta/360) · k_px pixels. Hardware
// (docs/notes/m1-wheel-scale.md): one detent = 360 units, one revolution
// = 4096 detents = 1,474,560 units; one full turn (at k_px = 0.25) moves
// 1024 px ≈ half a 1920-px screen.
class JogEngine {
 public:
  JogEngine(IWheelSource &source, IPlayheadSink &sink, IZoomSource &zoom,
            double pixels_per_detent = 0.0)
      : source_(source),
        sink_(sink),
        zoom_(zoom),
        pixels_per_detent_(pixels_per_detent) {}

  // Re-thread k_px (Session: after a bindings reload).
  void SetPixelsPerDetent(double pixels_per_detent) {
    pixels_per_detent_ = pixels_per_detent;
  }

  void Tick() {
    WheelSample ev;
    source_.Tick(&ev);
    if (ev.delta == 0) return;
    const double px_per_sec = zoom_.PixelsPerSecond();
    if (px_per_sec <= 0.0) return;  // degenerate zoom: no move
    sink_.ScrubBySeconds(
        (ev.delta / kUnitsPerDetent) * (pixels_per_detent_ / px_per_sec));
  }

  static constexpr double kUnitsPerDetent = 360.0;      // measured
  static constexpr double kUnitsPerRev = 1474560.0;     // 4096 * 360, measured

 private:
  IWheelSource &source_;
  IPlayheadSink &sink_;
  IZoomSource &zoom_;
  double pixels_per_detent_ = 0.0;  // k_px: pixels of timeline per detent
};

#endif // SPEED_REAPER_JOG_ENGINE_H
