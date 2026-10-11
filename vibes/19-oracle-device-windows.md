# Oracle (fake device + Windows)

Builds on `18-oracle-linux.md` (oracle green on Linux: `ORACLE_FULL=1 tools/check`).
Adds a scripted fake Speed Editor, so HID code is testable with no hardware
and no human, and the Windows layers (static check + `plugin_host.exe` under wine).
Design: `docs/plugin-oracle-design.md` §6 (fake device), §7 (Windows).

### 1. `IHidTransport` seam under the driver

- Create `src/speed_editor/hid_transport.h` (interface per design §6) and `src/speed_editor/hidapi_transport.{h,cpp}` (`HidapiTransport`, wraps the hidapi calls now made directly in `hid_device.cpp`).
- Change `SpeedEditorHID` to use an `IHidTransport` (constructor takes `std::unique_ptr<IHidTransport>`, default-constructs `HidapiTransport`); public API of `SpeedEditorHID` stays identical. Add `hidapi_transport.cpp` to `src/Makefile` `SRCS` and to the plugin's source list (Linux).
- Behavior must not change. Do not write the fake yet.

**Verify**
```bash
tools/check && make -C src && ./build/speedcli scan; test $? -le 1
```

- [ ] Done

### 2. `FakeTransport` and its script parser (pure)

- Create `src/speed_editor/fake_transport.{h,cpp}`: `FakeTransport(std::vector<ScriptEvent>)` plus `std::vector<ScriptEvent> ParseFakeScript(const std::string &text, std::string *err)` per design §6 (`at <ms> wheel <delta>`, `key <hex> down|up`, `unplug`, `plug`). Events are released by elapsed wall time since `Open`; `Read` returns the next due report (wheel = input report 0x03 with mode from the last `Write` of report 0x03, default 0; key = input report 0x04 with the full held set) or 0 on timeout; after `unplug`, `Read` returns −1 and `Open` fails until the matching `plug`.
- Implement the device side of auth (report 0x06) so `SpeedEditorHID::authenticate()` succeeds against it; reuse the algorithm already in `hid_device.cpp` (expose it as a free function in a small header if needed, no behavior change).
- Add `plugin/test/fake_transport_test.cpp`: parse good/bad scripts; a `SpeedEditorHID(FakeTransport)` opens, authenticates, `readEvent` yields the scripted wheel and key events in order, unplug → `readEvent` false + `Disconnected`, plug → reopen works. Add the new `.cpp` files to `PLUGIN_PURE_SOURCES`.
- Do not wire env selection yet.

**Verify**
```bash
tools/check fake_transport
```

- [ ] Done

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

### 4. Windows: static check and `plugin_host.exe`

- In `flake.nix`: add `wineWow64Packages.stable` (or the `wine64` equivalent present in the locked nixpkgs) to the devShell, and a package `plugin-host-windows` (mingw cross build of `plugin/tools/plugin_host.cpp` via the existing plugin derivation pattern, output `plugin_host.exe`, static runtime). Do not change the Linux outputs.
- Add `--platform windows` to `tools/reaper-oracle`: layer `static` (`x86_64-w64-mingw32-objdump -p` on the DLL: export table contains `ReaperPluginEntry`; record imports in `trace.txt`) and layer `host` (run `plugin_host.exe` on the DLL under `wine64` with `WINEPREFIX=build/oracle/wineprefix`, `WINEDEBUG=-all`, headless via `xvfb-run`; same `--expect-*` flags). `plugin_host` needs a `LoadLibrary` path under `#ifdef _WIN32`.
- If `plugin_host` cannot be built for Windows with the current code, record why in `docs/notes/oracle-limits.md` and make this task's only deliverable the static layer + that note.

**Verify**
```bash
tools/reaper-oracle --platform windows --layer static && tools/reaper-oracle --platform windows --layer host
```

- [ ] Done

### 5. Spike: real Windows REAPER under wine

- Goal: find out whether the full `reaper` layer can run on Windows via wine; not to make it work at any cost.
- Write `docs/notes/oracle-windows-reaper.md`: how to obtain the Windows REAPER installer reproducibly (URL pattern for 7.81 on reaper.fm, `nix-prefetch-url` hash, portable install `/PORTABLE`), what was tried (at most 3 attempts: portable install, run with `-nosplash` + the `load` scenario under xvfb+wine), the observed result for each, and a recommendation: *required*, *optional*, or *drop*.
- Do not add anything to the flake in this task.

**Verify**
```bash
test -s docs/notes/oracle-windows-reaper.md && grep -qiE '^Recommendation: (required|optional|drop)' docs/notes/oracle-windows-reaper.md
```

- [ ] Done

### 6. (human) Review the Windows recommendation

- [!] Done
  BLOCKED: human reads `docs/notes/oracle-windows-reaper.md`. If "required"/"optional", write follow-up tasks here to add the wine REAPER layer to `tools/reaper-oracle`; if "drop", tick this box. Linux milestones (M1+) do not wait for this.
