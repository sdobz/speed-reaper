// Characterizes today's JogEngine smoothing (no REAPER needed).
#include "check.h"
#include "jog_engine.h"

namespace {
struct FixedSource : IWheelSource {
  int d = 0;
  void Tick(WheelEvent *o) override { o->delta = d; }
};
struct SumSink : IPlayheadSink {
  double total = 0;
  void ScrubBySeconds(double dt) override { total += dt; }
  double PlayPosition() const override { return total; }
};
} // namespace

int main() {
  FixedSource src; SumSink sink;
  JogEngine eng(src, sink);
  src.d = 10;
  for (int i = 0; i < 100; ++i) eng.Tick(8.0);
  CHECK(sink.total > 0.0);
  src.d = 0;
  double before = sink.total;
  for (int i = 0; i < 100; ++i) eng.Tick(8.0); // coasts, then stops
  CHECK(sink.total > before);
  double settled = sink.total;
  for (int i = 0; i < 10; ++i) eng.Tick(8.0);
  CHECK_NEAR(sink.total, settled, 1e-9);
  return check_failures();
}
