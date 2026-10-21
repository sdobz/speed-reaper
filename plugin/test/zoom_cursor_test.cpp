// zoom_cursor_test.cpp — the zoom_cursor turn verb (46 §4.2, replacing
// select_step): each Step converts the raw wheel delta to signed detents
// (delta / 360), runs ZoomAround around the edit cursor, and writes the
// result via SetViewSpan. The arrange view is not an undoable edit: the
// hold never touches the undo block. No args are allowed.
#include <cmath>  // std::floor (pixel-snap fake)
#include <cstring>  // std::strlen (error-prefix check)
#include <string>
#include <vector>

#include "verb_registry.h"
#include "zoom.h"

#include "check.h"
#include "fake_project.h"

namespace {

// Records SetViewSpan calls (the zoom_cursor write seam).
class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { (void)cmd; }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override {}
  void Message(const char *msg) override { (void)msg; }
  void SetViewSpan(double L, double R) override { spans.push_back({L, R}); }

  std::vector<ViewRange> spans;
};

// Setup: 10 s span, cursor at 3.2 s (q = 0.32, inside the view),
// 100 px/s timeline, k_px 0.25 → f = 0.25 / (100 * 10) = 2.5e-4.
void test_cw_narrows_ccw_widens() {
  VerbRegistry reg = MakeDefaultRegistry();
  auto v = reg.Build("zoom_cursor", BindingGesture::Turn, {}).verb;
  CHECK(v != nullptr);

  FakeProject project;
  project.cursor = 3.2;
  project.view = {0.0, 10.0};
  project.pixels_per_second = 100.0;
  FakeSink s;
  VerbContext c{s, project};
  c.px_per_detent = 0.25;

  // CW: one detent (360 units) narrows the span around the cursor.
  const ViewRange in1 = ZoomAround({0.0, 10.0}, 3.2, 1.0, 0.25, 100.0);
  v->Step(c, 360);
  CHECK(s.spans.size() == 1);
  CHECK_NEAR(s.spans[0].L, in1.L, 1e-9);
  CHECK_NEAR(s.spans[0].R, in1.R, 1e-9);
  CHECK(s.spans[0].R - s.spans[0].L < 10.0);  // narrowed
  // The cursor keeps its fraction of the span (q = 0.32, unclamped).
  const double q0 = (3.2 - 0.0) / 10.0;
  const double q1 = (3.2 - s.spans[0].L) / (s.spans[0].R - s.spans[0].L);
  CHECK_NEAR(q1, q0, 1e-9);

  // CCW: one detent widens it.
  const ViewRange out1 = ZoomAround({0.0, 10.0}, 3.2, -1.0, 0.25, 100.0);
  v->Step(c, -360);
  CHECK(s.spans.size() == 2);
  CHECK_NEAR(s.spans[1].L, out1.L, 1e-9);
  CHECK_NEAR(s.spans[1].R, out1.R, 1e-9);
  CHECK(s.spans[1].R - s.spans[1].L > 10.0);  // widened

  // One full turn (4096 units) = 4096 / 360 detents.
  const ViewRange in_rev =
      ZoomAround({0.0, 10.0}, 3.2, 4096.0 / 360.0, 0.25, 100.0);
  v->Step(c, 4096);
  CHECK(s.spans.size() == 3);
  CHECK_NEAR(s.spans[2].L, in_rev.L, 1e-9);
  CHECK_NEAR(s.spans[2].R, in_rev.R, 1e-9);
  CHECK(s.spans[2].R - s.spans[2].L < in1.R - in1.L);  // further narrowed

  // No undo calls for the whole hold; Begin/End are no-ops.
  v->Begin(c);
  v->End(c);
  CHECK(project.undo_begins == 0);
  CHECK(project.undo_ends == 0);
  CHECK(s.spans.size() == 3);  // Begin/End wrote nothing
}

// The host (REAPER, headless or not) quantises each written view edge to
// the pixel grid, and one zoom step moves the span by far less than a
// pixel. Re-reading the snapped view inside a Step pins L and drifts the
// cursor's screen fraction; the verb must chain off its own ideal view
// captured at Begin. The fake below feeds every GetView() the 0.01 s
// snapped version of the last write — a drifting implementation would
// see q wander as the span shrinks beneath the grid.
void test_hold_ignores_snapped_readback() {
  VerbRegistry reg = MakeDefaultRegistry();
  auto v = reg.Build("zoom_cursor", BindingGesture::Turn, {}).verb;
  CHECK(v != nullptr);

  FakeProject project;
  project.cursor = 3.0;            // q = 0.3 of the 0-10 s view
  project.view = {0.0, 10.0};
  project.pixels_per_second = 100.0;
  FakeSink s;
  VerbContext c{s, project};
  c.px_per_detent = 0.25;

  const auto snap = [](double x) {
    return std::floor(x / 0.01 + 0.5) * 0.01;  // 0.01 s pixel grid
  };

  v->Begin(c);  // captures the ideal view: W = 10 s, q = 0.3

  const int kSteps = 100;
  ViewRange ideal{0.0, 10.0};  // the unsnapped ZoomAround chain
  for (int i = 0; i < kSteps; ++i) {
    v->Step(c, 360);
    ideal = ZoomAround(ideal, project.cursor, 1.0, 0.25, 100.0);
    // The host quantises the written edges to the pixel grid; the verb
    // must never look at this read-back.
    project.view = {snap(s.spans.back().L), snap(s.spans.back().R)};
  }
  v->End(c);
  CHECK(s.spans.size() == kSteps);

  const ViewRange last = s.spans.back();
  const double q0 = (3.0 - 0.0) / 10.0;
  const double q_last = (3.0 - last.L) / (last.R - last.L);
  CHECK_NEAR(q_last, q0, 1e-6);  // pivot fraction never drifted
  CHECK_NEAR(last.L, ideal.L, 1e-9);  // the unsnapped chain, not the grid
  CHECK_NEAR(last.R, ideal.R, 1e-9);
  CHECK(last.R - last.L < 10.0);  // 100 CW detents did zoom in

  // End released the hold: a later Step re-reads the (snapped) live view.
  const ViewRange expected =
      ZoomAround(project.view, project.cursor, 1.0, 0.25, 100.0);
  v->Step(c, 360);
  CHECK(s.spans.size() == kSteps + 1);
  CHECK_NEAR(s.spans.back().L, expected.L, 1e-9);
  CHECK_NEAR(s.spans.back().R, expected.R, 1e-9);
}

// No args allowed; one-shot gestures are rejected by the registry.
void test_args_rejected() {
  VerbRegistry reg = MakeDefaultRegistry();
  const VerbBuildResult r =
      reg.Build("zoom_cursor", BindingGesture::Turn, {"1"});
  CHECK(r.verb == nullptr);
  CHECK(!r.error.empty());
  CHECK(r.error.compare(0, std::strlen("zoom_cursor"), "zoom_cursor") == 0);
  CHECK(!reg.Build("zoom_cursor", BindingGesture::Tap, {}).error.empty());
  CHECK(!reg.Build("zoom_cursor", BindingGesture::Press, {}).error.empty());
}

}  // namespace

int main() {
  test_cw_narrows_ccw_widens();
  test_hold_ignores_snapped_readback();
  test_args_rejected();
  return check_failures();
}
