// select_track_test.cpp — the CAM N verb (docs/notes/cam-n-select.md):
// tap flips the selection bit of the item under the cursor on track N
// (1-based); additive across tracks; no item → no-op; cursor at the
// track's last-item end → that item; arg 0/10 → parse error; double/turn
// rejected; the state_read provider is true iff the under-cursor item on
// track N exists and is selected (follows selection and cursor moves),
// nullopt on a bad arg. No REAPER.
#include "verb_registry.h"

#include "check.h"
#include "fake_project.h"

#include <algorithm>  // std::any_of

namespace {

// select_track never runs a command; a minimal sink for the context.
class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { (void)cmd; }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override {}
  void Message(const char *msg) override { (void)msg; }
};

ItemInfo Item(int id, int track, double pos, double len, bool selected = false) {
  return {id, track, pos, len, selected};
}

// The FakeProject records SetSelectedItems but does not apply it; in
// REAPER the call applies to the live project, so the test applies the
// latest recorded call to items[].selected between taps.
void ApplySelection(FakeProject &p) {
  if (p.selected_calls.empty()) return;
  const auto &sel = p.selected_calls.back();
  for (auto &it : p.items)
    it.selected = std::any_of(sel.begin(), sel.end(),
                              [&it](int id) { return id == it.id; });
}

std::vector<int> SelectedIds(const FakeProject &p) {
  std::vector<int> out;
  for (const auto &it : p.items)
    if (it.selected) out.push_back(it.id);
  return out;
}

// One item on track 1 and one on track 2, both containing cursor 1.0.
FakeProject TwoTracks() {
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 2.0), Item(1, 1, 0.0, 2.0)};
  p.cursor = 1.0;
  return p;
}

void TestToggleOnOffOn() {
  const VerbRegistry reg = MakeDefaultRegistry();
  FakeProject p = TwoTracks();
  const auto res = reg.Build("select_track", BindingGesture::Tap, {"1"});
  CHECK(res.verb != nullptr);
  CHECK(res.error.empty());
  if (!res.verb) return;

  FakeSink sink;
  VerbContext ctx{sink, p};
  p.cursor = 1.0;  // inside item 0 (track 1)
  res.verb->Run(ctx);  // on
  ApplySelection(p);
  CHECK(SelectedIds(p) == (std::vector<int>{0}));
  res.verb->Run(ctx);  // off
  ApplySelection(p);
  CHECK(SelectedIds(p).empty());
  res.verb->Run(ctx);  // on again
  ApplySelection(p);
  CHECK(SelectedIds(p) == (std::vector<int>{0}));
  CHECK(p.undo_begins == 0);       // no undo block (selection, not an edit)
  CHECK(p.undo_ends == 0);
}

void TestAdditiveAcrossTracks() {
  const VerbRegistry reg = MakeDefaultRegistry();
  FakeProject p = TwoTracks();
  p.items[0].selected = true;  // track-1 item already selected
  const auto res = reg.Build("select_track", BindingGesture::Tap, {"2"});
  CHECK(res.verb != nullptr);
  if (!res.verb) return;
  FakeSink sink;
  VerbContext ctx{sink, p};
  res.verb->Run(ctx);  // select track-2 item, keep track-1's
  ApplySelection(p);
  CHECK(SelectedIds(p) == (std::vector<int>{0, 1}));
  res.verb->Run(ctx);  // toggle off: only track 1 remains
  ApplySelection(p);
  CHECK(SelectedIds(p) == (std::vector<int>{0}));
}

void TestNoItemNoOp() {
  const VerbRegistry reg = MakeDefaultRegistry();
  FakeProject p = TwoTracks();
  p.items = {Item(0, 0, 0.0, 2.0)};  // nothing on track 2
  p.cursor = 1.0;
  const auto res = reg.Build("select_track", BindingGesture::Tap, {"2"});
  CHECK(res.verb != nullptr);
  if (!res.verb) return;
  FakeSink sink;
  VerbContext ctx{sink, p};
  res.verb->Run(ctx);
  CHECK(p.selected_calls.empty());  // no-op: no selection write at all
  p.cursor = 5.0;  // and also a no-op when the cursor is off every item
  res.verb->Run(ctx);
  CHECK(p.selected_calls.empty());
}

void TestCursorAtLastItemEnd() {
  const VerbRegistry reg = MakeDefaultRegistry();
  FakeProject p;
  p.items = {Item(0, 0, 0.0, 2.0), Item(1, 0, 2.0, 2.0)};
  p.cursor = 4.0;  // exactly the end of track 1's last item → item 1
  const auto res = reg.Build("select_track", BindingGesture::Tap, {"1"});
  CHECK(res.verb != nullptr);
  if (!res.verb) return;
  FakeSink sink;
  VerbContext ctx{sink, p};
  res.verb->Run(ctx);
  CHECK(p.selected_calls.size() == 1);
  CHECK(p.selected_calls[0] == (std::vector<int>{1}));
}

void TestArgRange() {
  const VerbRegistry reg = MakeDefaultRegistry();
  const auto zero = reg.Build("select_track", BindingGesture::Tap, {"0"});
  CHECK(zero.verb == nullptr);
  CHECK(zero.error == "select_track: N must be 1..9");
  const auto ten = reg.Build("select_track", BindingGesture::Tap, {"10"});
  CHECK(ten.verb == nullptr);
  CHECK(ten.error == "select_track: N must be 1..9");
  const auto not_int = reg.Build("select_track", BindingGesture::Tap, {"x"});
  CHECK(not_int.verb == nullptr);
  CHECK(not_int.error ==
        "select_track: argument 'x' is not an integer");
}

void TestGestures() {
  const VerbRegistry reg = MakeDefaultRegistry();
  const auto dbl = reg.Build("select_track", BindingGesture::Double, {"1"});
  CHECK(dbl.verb == nullptr);
  CHECK(dbl.error == "verb 'select_track' does not accept gesture 'double'");
  const auto turn = reg.Build("select_track", BindingGesture::Turn, {"1"});
  CHECK(turn.verb == nullptr);
  CHECK(turn.error == "verb 'select_track' does not accept gesture 'turn'");
}

void TestStateRead() {
  const VerbRegistry reg = MakeDefaultRegistry();
  const auto read_opt = reg.StateReadFor("select_track", {"2"});
  CHECK(read_opt.has_value());
  if (!read_opt) return;
  const StateRead &read = *read_opt;

  FakeProject p = TwoTracks();
  LedReadContext ctx{p, false, false};
  CHECK(!read(ctx));  // track-2 item under the cursor, not selected

  p.items[1].selected = true;  // selection moves → lit
  CHECK(read(ctx));

  p.cursor = 5.0;              // cursor off the item → dark
  CHECK(!read(ctx));

  p.cursor = 1.0;              // cursor back on → lit
  CHECK(read(ctx));

  CHECK(!reg.StateReadFor("select_track", {"x"}).has_value());
  CHECK(!reg.StateReadFor("select_track", {"0"}).has_value());
}

}  // namespace

int main() {
  TestToggleOnOffOn();
  TestAdditiveAcrossTracks();
  TestNoItemNoOp();
  TestCursorAtLastItemEnd();
  TestArgRange();
  TestGestures();
  TestStateRead();
  return check_failures();
}
