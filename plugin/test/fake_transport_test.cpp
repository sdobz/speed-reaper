// FakeTransport (src/speed_editor/fake_transport.{h,cpp}) — the scripted
// fake Speed Editor device used by the oracle (docs/plugin-oracle-design.md
// §6). Checks: script parsing (good/bad), and a full SpeedEditorHID session
// over the fake: open, authenticate, scripted wheel + key events in order,
// unplug (readEvent false + Disconnected), plug (reopen works).
#include "check.h"
#include "speed_editor/fake_transport.h"
#include "speed_editor/hid_device.h"

#include <chrono>
#include <thread>

namespace {

using Ms = std::chrono::milliseconds;

void sleepMs(int ms) { std::this_thread::sleep_for(Ms(ms)); }

std::vector<ScriptEvent> goodScript() {
  // Comments and blank lines are ignored.
  const char *text =
      "# scripted session\n"
      "at 10 wheel 30\n"
      "\n"
      "at 25 key 0x3C down\n"
      "at 40 key 0x41 down   # two keys held\n"
      "at 55 key 0x3C up\n"
      "at 70 key 0x41 up\n"
      "at 200 unplug\n"
      "at 400 plug\n";
  std::string err;
  auto evs = ParseFakeScript(text, &err);
  CHECK(evs.size() == 8);
  if (evs.size() != 8) {
    std::fprintf(stderr, "parse error: %s\n", err.c_str());
  }
  CHECK(evs[0].kind == ScriptEvent::Kind::Wheel && evs[0].atMs == 10 && evs[0].delta == 30);
  CHECK(evs[1].kind == ScriptEvent::Kind::KeyDown && evs[1].keycode == 0x3C);
  CHECK(evs[2].kind == ScriptEvent::Kind::KeyDown && evs[2].keycode == 0x41);
  CHECK(evs[3].kind == ScriptEvent::Kind::KeyUp && evs[3].keycode == 0x3C);
  CHECK(evs[4].kind == ScriptEvent::Kind::KeyUp && evs[4].keycode == 0x41);
  CHECK(evs[5].kind == ScriptEvent::Kind::Unplug && evs[5].atMs == 200);
  CHECK(evs[6].kind == ScriptEvent::Kind::Plug && evs[6].atMs == 400);
  return evs;
}

void badScripts() {
  std::string err;
  struct Bad {
    const char *text;
    const char *why;
  } cases[] = {
      {"at 10 spin 3", "unknown verb"},
      {"at 10 wheel", "wheel without delta"},
      {"at 10 wheel many 3", "too many args"},
      {"at soon wheel 3", "non-numeric ms"},
      {"at -5 wheel 3", "negative ms"},
      {"at 10 wheel 3.5", "non-integer delta"},
      {"at 10 key 0x3C sideways", "bad key state"},
      {"at 10 key zz down", "bad keycode"},
      {"at 10 key 0x12345 down", "keycode out of range"},
      {"at 10 key 0x3C", "key missing state"},
      {"at 10 unplug now", "unplug with arg"},
      {"wheel 10", "missing 'at'"},
      {"at 10 plug extra", "plug with arg"},
      {"", "empty script is fine — no, it parses to 0 events (not checked here)"},
  };
  // The last "case" is actually valid (empty script); check it separately.
  for (size_t i = 0; i < 13; i++) {
    err.clear();
    auto evs = ParseFakeScript(cases[i].text, &err);
    CHECK(evs.empty());
    CHECK(!err.empty());
    if (err.empty()) {
      std::fprintf(stderr, "expected parse error for: %s\n", cases[i].text);
    }
  }
  err.clear();
  auto empty = ParseFakeScript("", &err);
  CHECK(empty.empty() && err.empty());
}

void fullSession() {
  auto evs = goodScript();
  SpeedEditorHID dev(std::make_unique<FakeTransport>(evs));
  CHECK(dev.open(""));
  CHECK(dev.getSerial() == "FAKE0001");
  CHECK(dev.authenticate());
  CHECK(dev.getState() == SpeedEditorHID::State::Connected);
  CHECK(dev.setWheelMode(2));  // wheel reports must echo mode 2

  // 1) wheel 30 @10ms, mode from the last 0x03 write
  SpeedEditorEvent e{};
  CHECK(dev.readEvent(e, 500));
  CHECK(e.type == EventType::Wheel);
  CHECK(e.wheel.delta == 30);
  CHECK(e.wheel.mode == 2);

  // 2) key 0x3C down @25ms
  CHECK(dev.readEvent(e, 500));
  CHECK(e.type == EventType::Button);
  CHECK(e.button.keycodes.size() == 1 && e.button.keycodes[0] == 0x3C);

  // 3) key 0x41 down @40ms — full held set, press order
  CHECK(dev.readEvent(e, 500));
  CHECK(e.type == EventType::Button);
  CHECK(e.button.keycodes.size() == 2 && e.button.keycodes[0] == 0x3C &&
        e.button.keycodes[1] == 0x41);

  // 4) key 0x3C up @55ms
  CHECK(dev.readEvent(e, 500));
  CHECK(e.type == EventType::Button);
  CHECK(e.button.keycodes.size() == 1 && e.button.keycodes[0] == 0x41);

  // 5) key 0x41 up @70ms — empty held set = release
  CHECK(dev.readEvent(e, 500));
  CHECK(e.type == EventType::Button);
  CHECK(e.button.isRelease());

  // 6) unplug @200ms: Read → −1 → readEvent false, device closed
  CHECK(!dev.readEvent(e, 600));
  CHECK(dev.getState() == SpeedEditorHID::State::Disconnected);

  // 7) Open still fails before the matching plug (plug @400ms)
  CHECK(!dev.open(""));

  // 8) once the plug time passes, reopen + re-auth work
  int tries = 0;
  while (!dev.open("") && tries++ < 100) {
    sleepMs(20);  // wait out the 400ms plug time, poll-friendly
  }
  CHECK(tries < 100);
  CHECK(dev.authenticate());
  CHECK(dev.getState() == SpeedEditorHID::State::Connected);
  // Script exhausted — readEvent times out (no events left).
  CHECK(!dev.readEvent(e, 100));
  CHECK(dev.getState() == SpeedEditorHID::State::Connected);
  dev.close();
}

void defaultWheelMode() {
  std::string err;
  auto evs = ParseFakeScript("at 5 wheel -7\n", &err);
  CHECK(evs.size() == 1 && err.empty());
  SpeedEditorHID dev(std::make_unique<FakeTransport>(evs));
  CHECK(dev.open(""));
  CHECK(dev.authenticate());
  // No setWheelMode() call → input reports carry mode 0.
  SpeedEditorEvent e{};
  CHECK(dev.readEvent(e, 300));
  CHECK(e.type == EventType::Wheel);
  CHECK(e.wheel.delta == -7);
  CHECK(e.wheel.mode == 0);
  dev.close();
}

}  // namespace

int main() {
  badScripts();
  fullSession();
  defaultWheelMode();
  return check_failures();
}
