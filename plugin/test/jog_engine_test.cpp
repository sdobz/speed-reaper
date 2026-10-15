// Characterizes JogEngine's 1:1 delta mapping (no REAPER needed).
#include "check.h"
#include "jog_engine.h"

namespace {
struct FixedSource : IWheelSource {
  int d = 0;
  void Tick(WheelSample *o) override { o->delta = d; }
};
struct SumSink : IPlayheadSink {
  double total = 0;
  void ScrubBySeconds(double dt) override { total += dt; }
  double PlayPosition() const override { return total; }
};
} // namespace

int main() {
  // 1:1 mapping: one revolution (measured units) = kSecondsPerRev, in either
  // direction; motion stops the instant the wheel does (no coast, no backlog).
  {
    FixedSource src; SumSink sink;
    JogEngine eng(src, sink);
    src.d = (int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, JogEngine::kSecondsPerRev, 1e-9);
    src.d = -(int)JogEngine::kUnitsPerRev;
    eng.Tick();
    CHECK_NEAR(sink.total, 0.0, 1e-9);
    src.d = 0;
    for (int i = 0; i < 10; ++i) eng.Tick();
    CHECK_NEAR(sink.total, 0.0, 1e-9);
  }

  return check_failures();
}
