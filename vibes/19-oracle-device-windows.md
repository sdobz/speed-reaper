# Fake device (Linux oracle)

Builds on `18-oracle-linux.md` (oracle green on Linux: `ORACLE_FULL=1 tools/check`).
Adds a scripted fake Speed Editor, so HID code is testable with no hardware
and no human, under the headless Linux REAPER oracle.
Design: `docs/plugin-oracle-design.md` §6 (fake device).
Windows/wine oracle layers are intentionally out of scope: if it works on Linux
we assume it works on Windows.

### 1. `IHidTransport` seam under the driver

- Create `src/speed_editor/hid_transport.h` (interface per design §6) and `src/speed_editor/hidapi_transport.{h,cpp}` (`HidapiTransport`, wraps the hidapi calls now made directly in `hid_device.cpp`).
- Change `SpeedEditorHID` to use an `IHidTransport` (constructor takes `std::unique_ptr<IHidTransport>`, default-constructs `HidapiTransport`); public API of `SpeedEditorHID` stays identical. Add `hidapi_transport.cpp` to `src/Makefile` `SRCS` and to the plugin's source list (Linux).
- Behavior must not change. Do not write the fake yet.

**Verify**
```bash
tools/check && make -C src && ./build/speedcli scan; test $? -le 1
```

- [x] Done

### 2. `FakeTransport` and its script parser (pure)

- Create `src/speed_editor/fake_transport.{h,cpp}`: `FakeTransport(std::vector<ScriptEvent>)` plus `std::vector<ScriptEvent> ParseFakeScript(const std::string &text, std::string *err)` per design §6 (`at <ms> wheel <delta>`, `key <hex> down|up`, `unplug`, `plug`). Events are released by elapsed wall time since `Open`; `Read` returns the next due report (wheel = input report 0x03 with mode from the last `Write` of report 0x03, default 0; key = input report 0x04 with the full held set) or 0 on timeout; after `unplug`, `Read` returns −1 and `Open` fails until the matching `plug`.
- Implement the device side of auth (report 0x06) so `SpeedEditorHID::authenticate()` succeeds against it; reuse the algorithm already in `hid_device.cpp` (expose it as a free function in a small header if needed, no behavior change).
- Add `plugin/test/fake_transport_test.cpp`: parse good/bad scripts; a `SpeedEditorHID(FakeTransport)` opens, authenticates, `readEvent` yields the scripted wheel and key events in order, unplug → `readEvent` false + `Disconnected`, plug → reopen works. Add the new `.cpp` files to `PLUGIN_PURE_SOURCES`.
- Do not wire env selection yet.

**Verify**
```bash
tools/check fake_transport
```

- [x] Fix the fake_transport_test expectation and make `tools/check fake_transport` green
  <!-- ralph: replanned=1 -->
  All files for this task already exist and compile (`src/speed_editor/fake_transport.{h,cpp}`, `src/speed_editor/auth.h`, `plugin/test/fake_transport_test.cpp`); do not rewrite them and do not touch `hid_device.*` or `plugin/CMakeLists.txt`.
  The one known bug: `plugin/test/fake_transport_test.cpp:33` checks `evs.size() == 8`, but the `goodScript()` script has 7 event lines and `evs[0]..evs[6]` are checked — change 8 to 7.
  `ParseFakeScript` in `src/speed_editor/fake_transport.cpp` already fills `*err` for all 13 bad-script cases (read it to confirm); the bad-case failures in `vibes/.ralph/logs/task2-verify-a3.log` predate the current parser — if any other CHECK still fails, fix the parser, not the expectations.
  Run `tools/check fake_transport` in the foreground (incremental build, ~2-4 min, it self-launches `nix develop`); when it exits 0, tick the box.
  Verify: `tools/check fake_transport`

### 3. Select the fake by env (test builds only)

- Add CMake option `SPEED_REAPER_FAKE_DEVICE` (default OFF) in `plugin/CMakeLists.txt` defining the macro of the same name for the plugin and tools. When ON, a small factory `std::unique_ptr<IHidTransport> MakeTransport()` (in `plugin/src/transport_factory.{h,cpp}`) returns a `FakeTransport` loaded from the script path in env `SPEED_REAPER_FAKE_DEVICE` if set, else `HidapiTransport`. When OFF it always returns `HidapiTransport` (no env lookup compiled in).
- `tools/check` already passes `-DSPEED_REAPER_FAKE_DEVICE=ON`.
- Add `oracle/fake/wheel_turn.txt` (e.g. `at 300 wheel 20` repeated every 8 ms for 500 ms, then 20 deltas decaying to 0 over 500 ms — a script generator one-liner kept as `oracle/fake/gen_wheel_turn.py` is fine).
- `hid_wheel_source.cpp` does not exist yet (milestone 1 task 5 creates it and must use `MakeTransport()`). Add a small unit test of the factory (ON build + env var pointing at a temp script → `Open` succeeds and a scripted wheel event is readable).

**Verify**
```bash
tools/check && test -s oracle/fake/wheel_turn.txt
```

- [ ] Done
