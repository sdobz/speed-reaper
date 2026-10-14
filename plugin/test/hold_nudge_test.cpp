// hold_nudge_test.cpp — HoldNudge accumulator (M7, design §4):
// tick→step conversion, signed remainders symmetric toward zero, and
// direction reversal neither losing nor inventing steps.
#include "hold_nudge.h"

#include "check.h"

int main() {
  // 3 ticks → 0 steps (remainder +3), +1 more → 1.
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

  return check_failures();
}
