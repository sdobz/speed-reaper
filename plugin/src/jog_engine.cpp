#include "jog_engine.h"

#include <cmath>

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
