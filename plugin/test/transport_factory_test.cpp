// MakeTransport (plugin/src/transport_factory.cpp) — env-based transport
// selection, test builds only (vibes/19-oracle-device-windows.md task 3).
// The test build defines SPEED_REAPER_FAKE_DEVICE, so the env lookup is
// compiled in: with the env var pointing at a temp script, Open succeeds and
// a scripted wheel event is readable; without it, a real transport comes
// back.
#include "check.h"
#include "transport_factory.h"
#include "speed_editor/hid_device.h"

#include <cstdlib>
#include <fstream>

namespace {

const char *kScriptPath = "/tmp/speed_reaper_transport_factory_test.txt";

void fakeViaEnv() {
  {
    std::ofstream out(kScriptPath);
    CHECK(out.good());
    out << "# factory test script\n"
        << "at 20 wheel 17\n";
  }
  setenv("SPEED_REAPER_FAKE_DEVICE", kScriptPath, 1);
  SpeedEditorHID dev(MakeTransport());
  CHECK(dev.open(""));
  CHECK(dev.authenticate());
  CHECK(dev.getState() == SpeedEditorHID::State::Connected);
  SpeedEditorEvent e{};
  CHECK(dev.readEvent(e, 500));
  CHECK(e.type == EventType::Wheel);
  CHECK(e.wheel.delta == 17);
  dev.close();
  std::remove(kScriptPath);
}

void realWithoutEnv() {
  unsetenv("SPEED_REAPER_FAKE_DEVICE");
  SpeedEditorHID dev(MakeTransport());  // always HidapiTransport
  // No (or any) hardware: open may fail or succeed; the factory just needs
  // to hand back a usable transport.
  (void)dev.open("");
  dev.close();
}

}  // namespace

int main() {
  fakeViaEnv();
  realWithoutEnv();
  return check_failures();
}
