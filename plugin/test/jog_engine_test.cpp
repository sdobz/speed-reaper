// Characterizes JogEngine's zoom-scaled 1:1 delta mapping (46 §4.1, no
// REAPER needed): screen-space-constant, one detent = k_px pixels at any
// zoom; motion stops the instant the wheel does (no coast, no backlog).
#include "check.h"
#include "jog_engine.h"

namespace {
struct FixedSource : IWheelSource {
  int d = 0;
  void Tick(WheelSample *o) override { o->delta = d; }
};
struct FixedZoom : IZoomSource {
  double px_per_sec = 100.0;
  double PixelsPerSecond() const override { return px_per_sec; }
};
struct SumSink : IPlayheadSink {
  double total = 0;
  void ScrubBySeconds(double dt) override { total += dt; }
  double PlayPosition() const override { return total; }
};
}  // namespace

int main() {
  // One revolution = k_px * 4096 / pxPerSec seconds (4096 detents, k_px
  // pixels each), in either direction; motion stops the instant the wheel
  // does (no coast, no backlog).
  const double k_px = 0.25;
  const double pxPerSec = 100.0;
  {
    FixedSource src;
    FixedZoom zoom;
    zoom.px_per_sec = pxPerSec;
    SumSink sink;
    JogEngine eng(src, sink, zoom, k_px);
    src.d = (int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, k_px * 4096 / pxPerSec, 1e-12);
    src.d = -(int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, 0.0, 1e-12);
    src.d = 0;
    for (int i = 0; i < 10; ++i) eng.Tick();
    CHECK_NEAR(sink.total, 0.0, 1e-12);
  }

  // Screen-space-constant: the same raw delta travels the same screen
  // distance (k_px per detent) at any zoom level.
  {
    FixedSource src;
    FixedZoom zoom;
    SumSink sink;
    JogEngine eng(src, sink, zoom, k_px);
    const int kDetent = (int)JogEngine::kUnitsPerDetent; // one detent = 360 units
    zoom.px_per_sec = 50.0;   // zoomed in
    src.d = 2 * kDetent;      // two detents
    eng.Tick();
    CHECK_NEAR(sink.total * zoom.px_per_sec, 2.0 * k_px, 1e-12);
    sink.total = 0.0;
    zoom.px_per_sec = 400.0;  // zoomed out 8x: pixels per detent unchanged
    src.d = 2 * kDetent;
    eng.Tick();
    CHECK_NEAR(sink.total * zoom.px_per_sec, 2.0 * k_px, 1e-12);
    CHECK_NEAR(sink.total, (2.0 * k_px) / 400.0, 1e-12);
  }

  // Guard: pxPerSec <= 0 (degenerate zoom read) → no move.
  {
    FixedSource src;
    FixedZoom zoom;
    zoom.px_per_sec = 0.0;
    SumSink sink;
    JogEngine eng(src, sink, zoom, k_px);
    src.d = (int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, 0.0, 1e-12);
    zoom.px_per_sec = -1.0;
    src.d = (int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, 0.0, 1e-12);
  }

  // SetPixelsPerDetent re-threads k_px (Session: after a bindings reload).
  {
    FixedSource src;
    FixedZoom zoom;
    SumSink sink;
    JogEngine eng(src, sink, zoom, k_px);
    eng.SetPixelsPerDetent(1.0);
    src.d = (int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, 1.0 * 4096 / pxPerSec, 1e-12);
  }

  return check_failures();
}
