// Characterizes today's JogEngine smoothing (no REAPER needed).
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

  // SummedWheelSource adds the deltas of both sources every tick.
  {
    FixedSource a, b;
    a.d = 7;
    b.d = -3;
    SummedWheelSource summed(a, b);
    WheelSample s;
    summed.Tick(&s);
    CHECK(s.delta == 4);

    // A triggered DemoWheelSource fed through the engine moves the sink
    // forward and eventually settles.
    DemoWheelSource demo;
    NullWheelSource null;
    SummedWheelSource src2(demo, null);
    SumSink sink2;
    JogEngine eng2(src2, sink2);
    CHECK(sink2.total == 0.0);
    demo.Trigger();
    for (int i = 0; i < 260; ++i) eng2.Tick(8.0); // turn (1.25 s) + 800 ms coast
    CHECK(sink2.total > 0.0);
    double settled2 = sink2.total;
    for (int i = 0; i < 10; ++i) eng2.Tick(8.0);
    CHECK_NEAR(sink2.total, settled2, 1e-9);
  }

  return check_failures();
}
