// zoom_test.cpp — pure zoom math (docs/jog-discovery.md §4.2): CW narrows,
// CCW widens; the pivot fraction q is preserved; the per-turn factor at a
// 1920-px window (4096 detents, k_px = 0.25) is ≈ 0.59 in / 1.7 out; a pivot
// outside the view clamps to the nearest edge; doubling k_px doubles the
// log-step.
#include "zoom.h"

#include "check.h"

namespace {
ViewRange V(double l, double r) { return ViewRange{l, r}; }
double W(ViewRange v) { return v.R - v.L; }
}  // namespace

int main() {
  // Setup: 10 s span on screen, 192 px/s ⇒ the window is 1920 px wide.
  const ViewRange view = V(0.0, 10.0);
  const double px_per_sec = 192.0;  // 10 s * 192 px/s = 1920 px
  const double k_px = 0.25;         // wheel.pixels_per_detent default

  // CW (positive detents) narrows the span (zoom in).
  {
    const ViewRange r = ZoomAround(view, 5.0, 4096.0, k_px, px_per_sec);
    CHECK(W(r) < W(view));
  }
  // CCW (negative detents) widens the span (zoom out).
  {
    const ViewRange r = ZoomAround(view, 5.0, -4096.0, k_px, px_per_sec);
    CHECK(W(r) > W(view));
  }
  // Zero detents: untouched.
  {
    const ViewRange r = ZoomAround(view, 5.0, 0.0, k_px, px_per_sec);
    CHECK(r.L == view.L && r.R == view.R);
  }

  // q is preserved: the pivot keeps its fraction of the span.
  {
    const double pivot = 3.2;  // q = 0.32
    const ViewRange in_cw = ZoomAround(view, pivot, 1000.0, k_px, px_per_sec);
    const ViewRange in_ccw = ZoomAround(view, pivot, -1000.0, k_px, px_per_sec);
    CHECK_NEAR((pivot - in_cw.L) / W(in_cw), 0.32, 1e-9);
    CHECK_NEAR((pivot - in_ccw.L) / W(in_ccw), 0.32, 1e-9);
  }

  // Per-turn factor at 1920 px, k_px = 0.25 (docs §4.2): one full turn
  // (4096 detents) multiplies the span by ≈ 0.59 (in) / ≈ 1.7 (out).
  {
    const double in = W(ZoomAround(view, 5.0, 4096.0, k_px, px_per_sec)) / W(view);
    const double out =
        W(ZoomAround(view, 5.0, -4096.0, k_px, px_per_sec)) / W(view);
    CHECK_NEAR(in, 0.59, 0.02);
    CHECK_NEAR(out, 1.7, 0.05);
    // Sanity: (1 ∓ k_px/1920)^4096 directly.
    const double f = k_px / (px_per_sec * W(view));
    CHECK_NEAR(in, std::pow(1.0 - f, 4096.0), 1e-12);
    CHECK_NEAR(out, std::pow(1.0 + f, 4096.0), 1e-12);
  }

  // Pivot left of the view clamps q to 0: the new view starts at the pivot.
  {
    const ViewRange r = ZoomAround(view, -2.0, 1000.0, k_px, px_per_sec);
    CHECK_NEAR(r.L, -2.0, 1e-12);
    CHECK_NEAR(r.R, -2.0 + W(r), 1e-12);
    CHECK(W(r) < W(view));  // still a CW narrowing
  }
  // Pivot right of the view clamps q to 1: the new view ends at the pivot.
  {
    const ViewRange r = ZoomAround(view, 40.0, -1000.0, k_px, px_per_sec);
    CHECK_NEAR(r.R, 40.0, 1e-12);
    CHECK(W(r) > W(view));  // still a CCW widening
  }

  // Doubling k_px doubles the log-step: log(W′/W) is linear in k_px.
  {
    const double d = 1000.0;
    const double log1 =
        std::log(W(ZoomAround(view, 5.0, d, k_px, px_per_sec)) / W(view));
    const double log2 =
        std::log(W(ZoomAround(view, 5.0, d, 2.0 * k_px, px_per_sec)) /
                 W(view));
    // log(W′/W) = d·log(1 ∓ f), so doubling k_px doubles the log-step up to
    // the O(f²) remainder (d·f² ≈ 1.7e-5 here); 1e-4 covers it.
    CHECK_NEAR(log2, 2.0 * log1, 1e-4);
    // And in the other direction too.
    const double log1n =
        std::log(W(ZoomAround(view, 5.0, -d, k_px, px_per_sec)) / W(view));
    const double log2n =
        std::log(W(ZoomAround(view, 5.0, -d, 2.0 * k_px, px_per_sec)) /
                 W(view));
    CHECK_NEAR(log2n, 2.0 * log1n, 1e-4);
  }

  // Degenerate f guard: k_px as big as the whole window (f = 1) clamps to
  // f < 1, so CW still narrows to a positive span instead of crossing zero.
  // (Only 2 detents: more would push the span below double epsilon at 5 s.)
  {
    const ViewRange r = ZoomAround(view, 5.0, 2.0, 1920.0, px_per_sec);
    CHECK(W(r) > 0.0 && W(r) < W(view));
  }

  return check_failures();
}
