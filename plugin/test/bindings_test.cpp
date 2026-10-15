// bindings_test.cpp — the bindings.ini parser: verb forms, error lines
// (with line numbers), tuning, and the derived double-tap set.
#include <string>
#include <vector>

#include "bindings.h"

#include "check.h"
#include "fake_project.h"
#include "speed_editor/key_map.h"

namespace {

class FakeSink : public IActionSink {
 public:
  void RunCommand(int cmd) override { commands.push_back(cmd); }
  void RunCommandByName(const std::string &name) override {
    named.push_back(name);
  }
  void ScrollBySeconds(double dt) override { (void)dt; }
  void ApplyShuttleRate(double rate) override { (void)rate; }
  void HardStop() override { ++hard_stops; }
  void Message(const char *msg) override { messages.push_back(msg); }

  std::vector<int> commands;
  std::vector<std::string> named;
  std::vector<std::string> messages;
  int hard_stops = 0;
};

// The shipped example file, as-is: it must parse with zero errors.
const char *kExample =
    "[tuning]\n"
    "double_tap_ms = 300\n"
    "hold_ms = 350\n"
    "\n"
    "[bindings]\n"
    "; KEY.gesture = verb [args]        ; comment\n"
    "SPLIT.tap        = action 40012              ; Item: Split items at edit or play cursor\n"
    "SPLIT.turn       = nudge move                ; hold SPLIT + wheel: move target items\n"
    "TRIM_IN.tap      = targeted 40511            ; trim left edge to cursor, on the cursor item\n"
    "TRIM_IN.turn     = nudge trim_in             ; hold + wheel: drag the left edge\n"
    "ESC.double       = action 40029              ; Edit: Undo\n"
    "JOG.press        = wheel jog\n"
    "STOP_PLAY.press  = transport play_stop\n";

// Numeric id: the binding is present, named, and the verb built.
void test_numeric_id() {
  const Bindings b =
      ParseBindings("[bindings]\nSPLIT.tap = action 40012\n",
                    MakeDefaultRegistry());
  CHECK(b.errors.empty());
  const auto it = b.bindings.find({KEY_SPLIT, BindingGesture::Tap});
  CHECK(it != b.bindings.end());
  if (it != b.bindings.end()) {
    CHECK(it->second.name == "action");
    CHECK((it->second.args == std::vector<std::string>{"40012"}));
    CHECK(it->second.verb != nullptr);
  }
}

// The `_named` command form runs through the sink by name.
void test_named_command() {
  const Bindings b = ParseBindings("[bindings]\nSPLIT.tap = action _RSabc\n",
                                   MakeDefaultRegistry());
  CHECK(b.errors.empty());
  const auto it = b.bindings.find({KEY_SPLIT, BindingGesture::Tap});
  CHECK(it != b.bindings.end());
  if (it == b.bindings.end()) return;
  CHECK((it->second.args == std::vector<std::string>{"_RSabc"}));
  FakeSink sink;
  FakeProject project;
  VerbContext ctx{sink, project};
  it->second.verb->Run(ctx);
  CHECK((sink.named == std::vector<std::string>{"_RSabc"}));
  CHECK(sink.commands.empty());
}

// targeted: same forms, built verb runs the id inside the undo block.
void test_targeted() {
  const Bindings b =
      ParseBindings("[bindings]\nTRIM_IN.tap = targeted 40511\n",
                    MakeDefaultRegistry());
  CHECK(b.errors.empty());
  const auto it = b.bindings.find({KEY_TRIM_IN, BindingGesture::Tap});
  CHECK(it != b.bindings.end());
  if (it != b.bindings.end()) {
    CHECK(it->second.name == "targeted");
    CHECK((it->second.args == std::vector<std::string>{"40511"}));
    CHECK(it->second.verb != nullptr);
  }
}

// Unknown key: error line names the key, binding absent.
void test_unknown_key() {
  const Bindings b =
      ParseBindings("[bindings]\nFOOBAR.tap = action 40012\n",
                    MakeDefaultRegistry());
  CHECK(b.bindings.empty());
  CHECK(b.errors.size() == 1);
  if (!b.errors.empty()) {
    CHECK(b.errors[0].rfind("bindings.ini:2: ", 0) == 0);
    CHECK(b.errors[0].find("unknown key 'FOOBAR'") != std::string::npos);
  }
}

// Unknown gesture: error line names the gesture.
void test_unknown_gesture() {
  const Bindings b =
      ParseBindings("[bindings]\nSPLIT.frobnicate = action 40012\n",
                    MakeDefaultRegistry());
  CHECK(b.bindings.empty());
  CHECK(b.errors.size() == 1);
  if (!b.errors.empty())
    CHECK(b.errors[0].find("unknown gesture 'frobnicate'") != std::string::npos);
}

// Unknown verb: the registry's error surfaces on the line.
void test_unknown_verb() {
  const Bindings b =
      ParseBindings("[bindings]\nSPLIT.tap = frobnicate 40012\n",
                    MakeDefaultRegistry());
  CHECK(b.bindings.empty());
  CHECK(b.errors.size() == 1);
  if (!b.errors.empty())
    CHECK(b.errors[0].find("unknown verb 'frobnicate'") != std::string::npos);
}

// A `press` key must stand alone — both orders error, on the later line.
void test_press_exclusive() {
  {
    const Bindings b = ParseBindings(
        "[bindings]\n"
        "JOG.press = wheel jog\n"
        "JOG.tap = action 40012\n",
        MakeDefaultRegistry());
    CHECK(b.errors.size() == 1);
    if (!b.errors.empty()) {
      CHECK(b.errors[0].rfind("bindings.ini:3: ", 0) == 0);
      CHECK(b.errors[0].find("press") != std::string::npos);
    }
    CHECK(b.bindings.count({KEY_JOG, BindingGesture::Press}) == 1);
    CHECK(b.bindings.count({KEY_JOG, BindingGesture::Tap}) == 0);
  }
  {
    const Bindings b = ParseBindings(
        "[bindings]\n"
        "JOG.tap = action 40012\n"
        "JOG.press = wheel jog\n",
        MakeDefaultRegistry());
    CHECK(b.errors.size() == 1);
    if (!b.errors.empty()) {
      CHECK(b.errors[0].rfind("bindings.ini:3: ", 0) == 0);
      CHECK(b.errors[0].find("press") != std::string::npos);
    }
    CHECK(b.bindings.count({KEY_JOG, BindingGesture::Tap}) == 1);
    CHECK(b.bindings.count({KEY_JOG, BindingGesture::Press}) == 0);
  }
}

// Tuning: values are read; missing keys keep the defaults.
void test_tuning() {
  {
    const Bindings b = ParseBindings(
        "[tuning]\n"
        "double_tap_ms = 250\n"
        "hold_ms = 400\n"
        "hold_ticks_per_step = 8\n",
        MakeDefaultRegistry());
    CHECK(b.errors.empty());
    CHECK(b.tuning.double_tap_ms == 250);
    CHECK(b.tuning.hold_ms == 400);
    CHECK(b.tuning.hold_ticks_per_step == 8);
  }
  {
    const Bindings b = ParseBindings("", MakeDefaultRegistry());
    CHECK(b.errors.empty());
    CHECK(b.tuning.double_tap_ms == 300);
    CHECK(b.tuning.hold_ms == 350);
    CHECK(b.tuning.hold_ticks_per_step == 4);
  }
  {
    const Bindings b = ParseBindings(
        "[tuning]\n"
        "double_tap_ms = fast\n"
        "wheels = 3\n",
        MakeDefaultRegistry());
    CHECK(b.errors.size() == 2);
    CHECK(b.tuning.double_tap_ms == 300);  // bad line skipped
    for (const std::string &e : b.errors)
      CHECK(e.rfind("bindings.ini:", 0) == 0);
  }
}

// The double-tap set is derived from the `.double` bindings alone.
void test_double_tap_set() {
  const Bindings b = ParseBindings(
      "[bindings]\n"
      "IN.double = action 40029\n"
      "OUT.double = ripple_delete\n"
      "TIMELINE.tap = select_cursor_item\n"
      "ESC.press = wheel jog\n",
      MakeDefaultRegistry());
  CHECK(b.errors.empty());
  CHECK((b.double_tap_keys == std::vector<std::uint16_t>{KEY_IN, KEY_OUT}));
}

// Comments, blanks, and whitespace never count as lines or break parsing.
void test_comments_whitespace() {
  const Bindings b = ParseBindings(
      "; whole-line comment\n"
      "# another\n"
      "\n"
      "   [ bindings ]   ; the section\n"
      "  SPLIT   . tap  =  action  40012  ; split\n",
      MakeDefaultRegistry());
  CHECK(b.errors.empty());
  CHECK(b.bindings.size() == 1);
  CHECK(b.bindings.count({KEY_SPLIT, BindingGesture::Tap}) == 1);
}

// Errors carry 1-based line numbers; good lines around them still parse.
void test_line_numbers() {
  // lines: 1 section, 2 ok, 3 unknown key, 4 unknown gesture, 5 ok
  const Bindings b = ParseBindings(
      "[bindings]\n"
      "SPLIT.tap = action 40012\n"
      "NOPE.tap = action 40012\n"
      "SPLIT.wrong = action 40012\n"
      "ESC.double = action 40029\n",
      MakeDefaultRegistry());
  CHECK(b.errors.size() == 2);
  if (b.errors.size() == 2) {
    CHECK(b.errors[0].rfind("bindings.ini:3: ", 0) == 0);
    CHECK(b.errors[1].rfind("bindings.ini:4: ", 0) == 0);
  }
  CHECK(b.bindings.size() == 2);
}

// The shipped example file parses clean, with the expected shape.
void test_example_file() {
  const Bindings b = ParseBindings(kExample, MakeDefaultRegistry());
  CHECK(b.errors.empty());
  CHECK(b.bindings.size() == 7);
  CHECK((b.double_tap_keys == std::vector<std::uint16_t>{KEY_ESC}));
  CHECK(b.tuning.double_tap_ms == 300);
  CHECK(b.tuning.hold_ms == 350);
  const auto turn = b.bindings.find({KEY_TRIM_IN, BindingGesture::Turn});
  CHECK(turn != b.bindings.end());
  if (turn != b.bindings.end())
    CHECK(turn->second.name == "nudge");
}

}  // namespace

int main() {
  test_numeric_id();
  test_named_command();
  test_targeted();
  test_unknown_key();
  test_unknown_gesture();
  test_unknown_verb();
  test_press_exclusive();
  test_tuning();
  test_double_tap_set();
  test_comments_whitespace();
  test_line_numbers();
  test_example_file();
  return check_failures();
}
