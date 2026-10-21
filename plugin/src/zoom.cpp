// zoom.cpp — see zoom.h. Pure math; no REAPER API.

#include "zoom.h"

#include <cmath>
#include <algorithm>

namespace {

// f must stay < 1 so both (1 − f)^d (CW) and (1 + f)^(−d) (CCW) are well
// defined for any detent count; at sane settings f ≈ k_px / window-pixels
// (≈ 1e-4), so this only ever bites degenerate configs (docs/jog-discovery.md
// §4.2).
constexpr double kMaxF = 0.99;

}  // namespace

ViewRange ZoomAround(ViewRange view, double pivot, double detents,
                     double k_px, double px_per_sec) {
  const double W = view.R - view.L;
  if (W <= 0.0 || px_per_sec <= 0.0 || k_px <= 0.0 || detents == 0.0) {
    return view;  // degenerate: no rescale
  }
  double f = k_px / (px_per_sec * W);
  if (!(f > 0.0)) return view;  // NaN / underflow: no rescale
  f = std::min(f, kMaxF);

  double W_prime;
  if (detents > 0.0) {
    W_prime = W * std::pow(1.0 - f, detents);  // CW: zoom in
  } else {
    W_prime = W * std::pow(1.0 + f, -detents);  // CCW: zoom out
  }

  const double q =
      std::clamp((pivot - view.L) / W, 0.0, 1.0);
  const double L_prime = pivot - q * W_prime;
  return ViewRange{L_prime, L_prime + W_prime};
}
