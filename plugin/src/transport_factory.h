#pragma once
// Factory for the Speed Editor HID transport. Test builds only: compiled
// with -DSPEED_REAPER_FAKE_DEVICE (CMake
// option of the same name, default OFF) when the SPEED_REAPER_FAKE_DEVICE env
// var may name a script; MakeTransport() then returns a FakeTransport loaded
// from it (see src/speed_editor/fake_transport.h). Without the macro — every
// production build — it always returns a real HidapiTransport and no env
// lookup is compiled in.
#include <memory>

#include "speed_editor/hid_transport.h"

std::unique_ptr<IHidTransport> MakeTransport();
