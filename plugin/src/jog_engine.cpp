#include "jog_engine.h"

#include <cmath>

#include "trace.h"

void DemoWheelSource::Trigger() { t_ = 0.001; }
void DemoWheelSource::Stop() { t_ = 0.0; }

void DemoWheelSource::Tick(WheelSample *out) {
  if (t_ <= 0.0) {
    out->delta = 0;
    return;
  }
  t_ += kTickMs / 1000.0;

  double total = kRampUpS + kHoldS + kRampDownS;
  if (t_ >= total) {
    t_ = 0.0; // turn finished
    out->delta = 0;
    return;
  }

  // Velocity profile: linear ramp up, flat hold, linear ramp down.
  double v = 1.0;
  if (t_ < kRampUpS) v = t_ / kRampUpS;
  else if (t_ > kRampUpS + kHoldS)
    v = (total - t_) / kRampDownS;

  out->delta = (std::int32_t)std::llround(v * kMaxDeltaPerTick);
}

void JogEngine::Tick(double dt_ms) {
  WheelSample ev;
  source_.Tick(&ev);

  // Instantaneous wheel rate (units per ms) from this tick's delta.
  const double target = ev.delta / dt_ms;

  // Exponential low-pass toward the target rate → smooth acceleration and
  // coast-down when the wheel stops.
  const double tau_ms = 50.0;
  const double k = 1.0 - std::exp(-dt_ms / tau_ms);
  vel_ += (target - vel_) * k;
  if (std::fabs(vel_) < 1e-9) vel_ = 0.0;

  const double dt_s = vel_ * dt_ms * kSecondsPerWheelUnit;
  if (dt_s != 0.0) {
    // Diagnostic (M1 task 9): every engine step that moves the playhead,
    // with the raw input delta and the smoothed velocity. Continuous lines
    // while the wheel sits still = drift entering the engine.
    Trace("jog in=%d dt=%.5f vel=%.4f", ev.delta, dt_s, vel_);
    sink_.ScrubBySeconds(dt_s);
  }
}
