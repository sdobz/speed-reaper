// nudge.cpp — pure hold+rotate nudge math (M7, design §4).
//
// Every verb moves an edge (or offset) by steps * step_seconds; before
// applying, the raw delta is clamped so the invariants hold:
//   - item length stays >= one step,
//   - the active take's start offset stays >= 0.
// If the two clamps exclude every delta (pathological geometry), the item
// is returned unchanged.
#include "nudge.h"

#include <cmath>
#include <cfloat>

namespace {

constexpr double kNegInf = -DBL_MAX;
constexpr double kPosInf = DBL_MAX;

struct Range {
  double lo = kNegInf;
  double hi = kPosInf;
};

Range &Restrict(Range &r, double lo, double hi) {
  if (lo > r.lo) r.lo = lo;
  if (hi < r.hi) r.hi = hi;
  return r;
}

// Restrict s so that startoffs (+|-) s * rate stays >= 0.
// plus=true:  startoffs + s*rate >= 0 (trim-in / slip-src content)
// plus=false: startoffs - s*rate >= 0 (slip-dest content)
void StartoffsRestrict(Range &r, double startoffs, double rate, bool plus) {
  if (rate == 0.0) return;  // no content shift
  if (plus) {
    if (rate > 0.0)
      Restrict(r, -startoffs / rate, kPosInf);  // s >= bound
    else
      Restrict(r, kNegInf, -startoffs / rate);  // s <= bound
  } else {
    if (rate > 0.0)
      Restrict(r, kNegInf, startoffs / rate);   // s <= bound
    else
      Restrict(r, startoffs / rate, kPosInf);   // s >= bound
  }
}

// Pick the applied delta: raw if inside [lo, hi], the nearest bound
// otherwise; no-op if the range is empty (conflicting clamps).
double PickDelta(Range r, double raw) {
  if (r.hi < r.lo) return 0.0;
  if (raw < r.lo) return r.lo;
  if (raw > r.hi) return r.hi;
  return raw;
}

}  // namespace

ItemGeom NudgeMove(const ItemGeom &g, int steps, double step_seconds) {
  ItemGeom r = g;
  r.pos += static_cast<double>(steps) * step_seconds;
  return r;
}

ItemGeom NudgeTrimIn(const ItemGeom &g, int steps, double step_seconds) {
  const double raw = static_cast<double>(steps) * step_seconds;
  Range r;
  Restrict(r, kNegInf, g.len - step_seconds);  // len - s >= one step
  StartoffsRestrict(r, g.startoffs, g.playrate, true);
  const double s = PickDelta(r, raw);
  ItemGeom out = g;
  out.pos += s;
  out.len -= s;
  out.startoffs += s * g.playrate;
  return out;
}

ItemGeom NudgeTrimOut(const ItemGeom &g, int steps, double step_seconds) {
  const double raw = static_cast<double>(steps) * step_seconds;
  Range r;
  Restrict(r, step_seconds - g.len, kPosInf);  // len + s >= one step
  const double s = PickDelta(r, raw);
  ItemGeom out = g;
  out.len += s;
  return out;
}

ItemGeom NudgeSlipSrc(const ItemGeom &g, int steps, double step_seconds) {
  const double raw = static_cast<double>(steps) * step_seconds;
  Range r;
  StartoffsRestrict(r, g.startoffs, g.playrate, true);
  const double s = PickDelta(r, raw);
  ItemGeom out = g;
  out.startoffs += s * g.playrate;
  return out;
}

ItemGeom NudgeSlipDest(const ItemGeom &g, int steps, double step_seconds) {
  const double raw = static_cast<double>(steps) * step_seconds;
  Range r;
  StartoffsRestrict(r, g.startoffs, g.playrate, false);
  const double s = PickDelta(r, raw);
  ItemGeom out = g;
  out.pos += s;
  out.startoffs -= s * g.playrate;
  return out;
}

ItemGeom NudgeVolume(const ItemGeom &g, int steps, double step_seconds) {
  (void)step_seconds;
  ItemGeom out = g;
  // dB = 20 log10(vol); a non-positive vol (muted) counts as -infinity.
  const double db = g.vol > 0.0 ? 20.0 * std::log10(g.vol) : -1e300;
  const double nudged = db + 0.5 * static_cast<double>(steps);
  const double clamped = nudged < 0.0 ? 0.0 : (nudged > 12.0 ? 12.0 : nudged);
  out.vol = std::pow(10.0, clamped / 20.0);
  return out;
}
