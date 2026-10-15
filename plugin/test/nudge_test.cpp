// nudge_test.cpp — pure hold+rotate nudge math (M7, design §4): one happy
// assertion pair per verb function, plus the edge clamps (length >= one
// step, start offset >= 0) and non-unit playrates; and the HoldNudge
// accumulator: tick→step conversion, signed remainders symmetric toward
// zero, and direction reversal neither losing nor inventing steps.
#include "nudge.h"

#include "check.h"

namespace {
ItemGeom G(double pos, double len, double startoffs, double playrate,
           double vol) {
  return ItemGeom{pos, len, startoffs, playrate, vol};
}
}  // namespace

int main() {
  const double step = 0.1;

  // NudgeMove: pos += steps * step (positive and negative).
  {
    const ItemGeom g = G(10.0, 5.0, 2.0, 1.0, 1.0);
    const ItemGeom r = NudgeMove(g, 3, step);
    CHECK_NEAR(r.pos, 10.3, 1e-9);
    CHECK(r.len == 5.0 && r.startoffs == 2.0);
    const ItemGeom r2 = NudgeMove(g, -1, step);
    CHECK_NEAR(r2.pos, 9.9, 1e-9);
    CHECK(r2.len == 5.0 && r2.startoffs == 2.0);
  }

  // NudgeTrimIn (playrate 2.0): left edge by s, content stays
  // (startoffs += s * playrate).
  {
    const ItemGeom g = G(10.0, 5.0, 2.0, 2.0, 1.0);
    const ItemGeom r = NudgeTrimIn(g, 3, step);
    CHECK_NEAR(r.pos, 10.3, 1e-9);
    CHECK_NEAR(r.len, 4.7, 1e-9);
    CHECK_NEAR(r.startoffs, 2.6, 1e-9);  // 2 + 0.3*2
  }

  // NudgeTrimIn clamps: len >= one step and startoffs >= 0.
  {
    // len 0.3: +3 wants s=0.3, clamped to len-step=0.2.
    const ItemGeom g = G(10.0, 0.3, 0.0, 2.0, 1.0);
    const ItemGeom r = NudgeTrimIn(g, 3, step);
    CHECK_NEAR(r.len, step, 1e-9);
    CHECK_NEAR(r.pos, 10.2, 1e-9);
    CHECK_NEAR(r.startoffs, 0.4, 1e-9);  // 0 + 0.2*2
    // startoffs 0.15: -3 wants s=-0.3, clamped to -startoffs/playrate.
    const ItemGeom g2 = G(10.0, 5.0, 0.15, 2.0, 1.0);
    const ItemGeom r2 = NudgeTrimIn(g2, -3, step);
    CHECK_NEAR(r2.startoffs, 0.0, 1e-9);
    CHECK_NEAR(r2.pos, 10.0 - 0.075, 1e-9);
    CHECK_NEAR(r2.len, 5.0 + 0.075, 1e-9);
  }

  // NudgeTrimOut: len += steps * step; clamped to len >= one step.
  {
    const ItemGeom g = G(10.0, 5.0, 0.0, 1.5, 1.0);
    const ItemGeom r = NudgeTrimOut(g, 2, step);
    CHECK_NEAR(r.len, 5.2, 1e-9);
    CHECK(r.pos == 10.0 && r.startoffs == 0.0);
    // len 0.35: -5 wants s=-0.5, clamped to step-len = -0.25.
    const ItemGeom g2 = G(10.0, 0.35, 0.0, 1.5, 1.0);
    const ItemGeom r2 = NudgeTrimOut(g2, -5, step);
    CHECK_NEAR(r2.len, step, 1e-9);
    CHECK(r2.pos == 10.0 && r2.startoffs == 0.0);
  }

  // NudgeSlipSrc (playrate 2.0): startoffs += s * playrate, pos/len fixed.
  {
    const ItemGeom g = G(10.0, 5.0, 2.0, 2.0, 1.0);
    const ItemGeom r = NudgeSlipSrc(g, 3, step);
    CHECK_NEAR(r.startoffs, 2.6, 1e-9);  // 2 + 0.3*2
    CHECK(r.pos == 10.0 && r.len == 5.0);
    // startoffs 0.3: -5 wants s=-0.5, clamped to -startoffs/playrate = -0.15.
    const ItemGeom g2 = G(10.0, 5.0, 0.3, 2.0, 1.0);
    const ItemGeom r2 = NudgeSlipSrc(g2, -5, step);
    CHECK_NEAR(r2.startoffs, 0.0, 1e-9);
    CHECK(r2.pos == 10.0 && r2.len == 5.0);
  }

  // NudgeSlipDest (playrate 1.5): pos += s, startoffs -= s * playrate.
  {
    const ItemGeom g = G(10.0, 5.0, 2.0, 1.5, 1.0);
    const ItemGeom r = NudgeSlipDest(g, 2, step);
    CHECK_NEAR(r.pos, 10.2, 1e-9);
    CHECK_NEAR(r.startoffs, 1.7, 1e-9);  // 2 - 0.2*1.5
    // startoffs 0.2: +5 wants s=0.5, clamped to startoffs/playrate.
    const ItemGeom g2 = G(10.0, 5.0, 0.2, 1.5, 1.0);
    const ItemGeom r2 = NudgeSlipDest(g2, 5, step);
    CHECK_NEAR(r2.startoffs, 0.0, 1e-9);
    CHECK_NEAR(r2.pos, 10.0 + 0.2 / 1.5, 1e-9);
  }

  // NudgeVolume: 0.5 dB/step, clamped to 0 … +12 dB (vol linear).
  {
    const ItemGeom g = G(0.0, 0.0, 0.0, 1.0, 1.0);  // 0 dB
    const ItemGeom r = NudgeVolume(g, 2, step);
    CHECK_NEAR(r.vol, std::pow(10.0, 1.0 / 20.0), 1e-9);  // +1 dB
    const ItemGeom r2 = NudgeVolume(g, -2, step);
    CHECK_NEAR(r2.vol, 1.0, 1e-9);  // floor clamp: 0 dB
    const ItemGeom g3 = G(0.0, 0.0, 0.0, 1.0, std::pow(10.0, 11.0 / 20.0));
    const ItemGeom r3 = NudgeVolume(g3, 10, step);  // 11 + 5 -> 16 dB
    CHECK_NEAR(r3.vol, std::pow(10.0, 12.0 / 20.0), 1e-9);  // ceiling
  }

  // HoldNudge: 3 ticks → 0 steps (remainder +3), +1 more → 1.
  {
    HoldNudge n;
    CHECK(n.Add(1) == 0);
    CHECK(n.Add(1) == 0);
    CHECK(n.Add(1) == 0);
    CHECK(n.Add(1) == 1);
  }

  // −4 ticks in one go → −1 step.
  {
    HoldNudge n;
    CHECK(n.Add(-4) == -1);
    CHECK(n.Remainder() == 0);
  }

  // 10 ticks → 2 steps remainder 2, then +2 → 1 step.
  {
    HoldNudge n;
    CHECK(n.Add(10) == 2);
    CHECK(n.Remainder() == 2);
    CHECK(n.Add(2) == 1);
    CHECK(n.Remainder() == 0);
  }

  // Negative counterpart: −10 ticks → −2 steps remainder −2, then −2 → −1.
  {
    HoldNudge n;
    CHECK(n.Add(-10) == -2);
    CHECK(n.Remainder() == -2);
    CHECK(n.Add(-2) == -1);
    CHECK(n.Remainder() == 0);
  }

  // Direction reversal does not lose or invent steps: +3 then −3 is 0
  // total, +3 then −2 leaves remainder +1 (still 0 total steps).
  {
    HoldNudge n;
    CHECK(n.Add(3) == 0);
    CHECK(n.Add(-3) == 0);
    CHECK(n.Remainder() == 0);
  }
  {
    HoldNudge n;
    CHECK(n.Add(3) == 0);
    CHECK(n.Add(-2) == 0);
    CHECK(n.Remainder() == 1);   // reversal ate 2 of the 3, none lost
    CHECK(n.Add(1) == 0);        // net 2 ticks: still no step
    CHECK(n.Remainder() == 2);
    CHECK(n.Add(2) == 1);        // +2 finishes the original +4
  }
  {
    HoldNudge n;
    CHECK(n.Add(-3) == 0);
    CHECK(n.Add(2) == 0);
    CHECK(n.Remainder() == -1);  // reversal ate 2 of the 3, none lost
    CHECK(n.Add(-1) == 0);       // net −2 ticks: still no step
    CHECK(n.Remainder() == -2);
    CHECK(n.Add(-2) == -1);      // −2 finishes the original −4
  }

  // Reversal after completed steps: +5 → 1 step (rem +1), then −6 → −1 step
  // (rem −1); net ticks −1, net steps 0.
  {
    HoldNudge n;
    CHECK(n.Add(5) == 1);
    CHECK(n.Add(-6) == -1);
    CHECK(n.Remainder() == -1);  // net ticks = −1
    CHECK(n.Add(1) == 0);        // back to zero remainder, zero new steps
    CHECK(n.Remainder() == 0);
  }

  // Custom ticks_per_step: 1 tick → 1 step each.
  {
    HoldNudge n(1);
    CHECK(n.Add(1) == 1);
    CHECK(n.Add(-1) == -1);
  }

  // Fades: length += steps*step, clamped to 0 ... item length; the rest
  // of the geometry is untouched.
  {
    ItemGeom g = G(10.0, 1.0, 2.0, 1.0, 1.0);
    ItemGeom r = NudgeFadeIn(g, 3, step);
    CHECK_NEAR(r.fadein, 0.3, 1e-9);
    CHECK(r.pos == 10.0 && r.len == 1.0 && r.fadeout == 0.0);
    CHECK_NEAR(NudgeFadeIn(r, -9, step).fadein, 0.0, 1e-12);   // clamp >= 0
    CHECK_NEAR(NudgeFadeIn(g, 50, step).fadein, 1.0, 1e-12);   // clamp <= len
    g.fadeout = 0.2;
    CHECK_NEAR(NudgeFadeOut(g, 2, step).fadeout, 0.4, 1e-9);
    CHECK(NudgeFadeOut(g, 2, step).fadein == 0.0);
    CHECK_NEAR(NudgeFadeOut(g, -5, step).fadeout, 0.0, 1e-12);
  }

  // Roll: the shared edit point moves; a.len + b.len stays fixed and b's
  // content stays in time (startoffs += d*playrate).
  {
    ItemGeom a = G(0.0, 4.0, 0.0, 1.0, 1.0);
    ItemGeom b = G(4.0, 6.0, 1.0, 2.0, 1.0);
    NudgeRoll(&a, &b, 3, step);
    CHECK_NEAR(a.len, 4.3, 1e-9);
    CHECK_NEAR(b.pos, 4.3, 1e-9);
    CHECK_NEAR(b.len, 5.7, 1e-9);
    CHECK_NEAR(a.len + b.len, 10.0, 1e-9);
    CHECK_NEAR(b.startoffs, 1.6, 1e-9);  // 1 + 0.3*2
    CHECK(a.pos == 0.0);
    // Clamps: b can shrink to one step; a can shrink to one step.
    NudgeRoll(&a, &b, 1000, step);
    CHECK_NEAR(b.len, step, 1e-9);
    CHECK_NEAR(a.len + b.len, 10.0, 1e-9);
    NudgeRoll(&a, &b, -1000, step);
    CHECK(a.len >= step - 1e-9);
    CHECK(b.startoffs >= -1e-9);
    CHECK_NEAR(a.len + b.len, 10.0, 1e-9);
  }

  // Time selection edges.
  {
    CHECK_NEAR(NudgeSelIn(2.0, 5.0, 3, step), 2.3, 1e-9);
    CHECK_NEAR(NudgeSelIn(2.0, 5.0, 100, step), 5.0, 1e-9);   // <= end
    CHECK_NEAR(NudgeSelIn(0.1, 5.0, -100, step), 0.0, 1e-9);  // >= 0
    CHECK_NEAR(NudgeSelOut(2.0, 5.0, -4, step), 4.6, 1e-9);
    CHECK_NEAR(NudgeSelOut(2.0, 5.0, -100, step), 2.0, 1e-9);  // >= start
  }

  return check_failures();
}
