// zoom.h — pure zoom math for TIMELINE-hold + wheel (docs/jog-discovery.md
// §4.2). No REAPER API, no HID: takes the arrange view span in seconds and
// the live timeline scale (pixels per second) and returns the new span after
// the wheel detents, re-anchored at the pivot (edit cursor). The REAPER
// glue (a later task) reads the inputs via GetSet_ArrangeView2 /
// GetCursorPosition / GetHZoomLevel and writes the result back.

#ifndef SPEED_REAPER_ZOOM_H
#define SPEED_REAPER_ZOOM_H

// Arrange view span, seconds: L = left edge, R = right edge, W = R − L.
struct ViewRange {
  double L = 0.0;
  double R = 0.0;
};

// Zoom the view around `pivot` (the edit cursor, seconds) by `detents`
// signed jog detents (docs/jog-discovery.md §4.2: one detent = 360 raw
// units; CW/positive = zoom in, CCW/negative = zoom out).
//
//   W    = R − L
//   f    = k_px / (px_per_sec · W)   clamped < 1 (degenerate configs)
//   CW   (detents > 0):  W′ = W · (1 − f)^detents
//   CCW  (detents < 0):  W′ = W · (1 + f)^(−detents)
//   q    = clamp((pivot − L) / W, 0, 1)
//   L′   = pivot − q · W′;  R′ = L′ + W′
//
// `k_px` is `wheel.pixels_per_detent` (screen pixels one detent should
// traverse); `px_per_sec` is the live timeline scale, so the per-detent
// step is screen-space-constant and doubling k_px doubles the zoom step.
// A pivot outside the view clamps q to the nearest edge, so the new view
// starts (or ends) at the pivot. Degenerate inputs (W <= 0, px_per_sec <=
// 0, k_px <= 0, detents == 0) leave the span width unchanged (and the
// anchor when detents == 0).
ViewRange ZoomAround(ViewRange view, double pivot, double detents,
                     double k_px, double px_per_sec);

#endif  // SPEED_REAPER_ZOOM_H
