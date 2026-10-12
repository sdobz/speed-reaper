# Milestone 1 — The real wheel moves the REAPER playhead

Replace the synthetic demo wheel with the real Speed Editor wheel, through
the existing `IWheelSource → JogEngine → IPlayheadSink` pipeline. First
usable plugin: plug in, turn the wheel, the playhead glides.
**Prerequisite:** `18-oracle-linux.md` and `19-oracle-device-windows.md` are done (`ORACLE_FULL=1 tools/check` is green; the fake device exists). Runtime behavior is verified by the oracle, not by hand; the human only does the final real-device test.
Design (names, signatures, threading) is settled in
`docs/plugin-design-m1-m3.md` — read the section each task names, don't redesign.
Every Verify starts with `tools/check` (build -Werror, `nm` export check, unit tests).

## Context
- `docs/plugin-design-m1-m3.md` — §1 layout, §2 threading, §3 naming, §4 HidWheelSource, §5 probe
- `src/speed_editor/hid_device.h` — the driver API (do not modify the driver); `docs/plugin-oracle-design.md` §2, §4, §6 — trace, scenarios, fake device
- `plugin/src/jog_engine.{h,cpp}`, `plugin/src/main.cpp` — existing pipeline

### 1. Rename the plugin's `WheelEvent` to `WheelSample`

- In `plugin/src/jog_engine.{h,cpp}` and `plugin/test/jog_engine_test.cpp`, rename struct `WheelEvent` → `WheelSample` (see design §3). Nothing else changes.
- Do not touch `src/speed_editor/`. Do not add features.

**Verify**
```bash
tools/check jog_engine && ! grep -rn "WheelEvent" plugin/src plugin/test
```

- [x] Done

### 2. Fix the demo wiring: `SummedWheelSource`

Today `JogEngine` ticks only its constructor `source_`, while `ToggleDemo()` drives a *separate* internal `DemoWheelSource`, so the demo action cannot move the playhead. Fix so both real and demo sources can feed the engine.

- In `plugin/src/jog_engine.h/.cpp`: remove `JogEngine::demo_`, `demo_on_`, `ToggleDemo()`, `DemoActive()`, `Demo()`.
- Add `class SummedWheelSource : public IWheelSource` with `SummedWheelSource(IWheelSource &a, IWheelSource &b)`; `Tick` ticks both and sums the deltas.
- Add `struct NullWheelSource : IWheelSource` (delta always 0) in `jog_engine.h`.
- In `plugin/src/main.cpp`: keep `g_demo_source`; the engine is built on `SummedWheelSource(g_demo_source, g_null)` (`g_null` is a `NullWheelSource`; task 6 swaps it for the HID source). The demo action calls `g_demo_source.Trigger()` / `Stop()` directly (stop if `Active()`, else trigger).
- Add to `plugin/test/jog_engine_test.cpp`: a test that `SummedWheelSource` sums two fixed sources, and that a triggered `DemoWheelSource` through the engine moves a fake sink forward.
- Do not add the HID source yet.

**Verify**
```bash
tools/check jog_engine
```

- [x] Done

### 3. Wire the driver and hidapi into the plugin build (stub `HidWheelSource`)

- Create `plugin/src/hid_wheel_source.h` with the exact public interface from design §4 (include `jog_engine.h`, `<atomic>`, `<thread>`, `<mutex>`, `<deque>`, `<string>`, and the driver's `hid_device.h` in the .cpp only).
- Create `plugin/src/hid_wheel_source.cpp` as a **stub**: the class holds a `SpeedEditorHID dev_` member; `Start/Stop/PopButton/PopLog/Request*` do nothing (`Pop*` return false); `Tick` sets `delta = 0`; `Connected()` false; `AbsolutePosition()` 0.
- `plugin/CMakeLists.txt` (Linux only, inside `if(UNIX AND NOT APPLE)`): add `${REPO_ROOT}/src/speed_editor/hid_device.cpp` and `hid_wheel_source.cpp` to the **plugin target and** `PLUGIN_PURE_SOURCES` (they are REAPER-free); include dir `${REPO_ROOT}/src`; `pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi-hidraw)` and link `PkgConfig::HIDAPI` to the plugin and to each test (`plugin/test/CMakeLists.txt`). `-Wall -Wextra` must stay warning-free for our code (`SYSTEM` for hidapi headers).
- Do not implement the worker thread. Do not edit `main.cpp`.

**Verify**
```bash
tools/check && readelf -d build/plugin/reaper_speed_reaper.so | grep -q hidapi
```

- [x] Done

### 4. Probe tool `hid_probe`

- Create `plugin/tools/hid_probe.cpp` per design §5 and add executable target `hid_probe` in `plugin/CMakeLists.txt` (linking the same sources as the plugin minus `main.cpp`; output in `build/plugin/`).
- It must work against the stub from task 3: with the stub it prints only the `wheel: sum=0` line and exits 0.
- Do not implement the worker thread.

**Verify**
```bash
tools/check && ./build/plugin/hid_probe --seconds 2 | grep -q 'wheel: sum=0'
```

- [x] Done

### 5. Implement the `HidWheelSource` worker thread

- Replace the stub in `plugin/src/hid_wheel_source.cpp` with the real worker loop **exactly** as design §4 steps 1–5 and §2 (atomic delta accumulator, mutex queues for logs/buttons, fixed log strings). `Stop()` must join in <1 s even if no device is present.
- Button diffing: implement the held-set diff as a private static helper for now (it moves to `button_diff.h` in milestone 2). Buttons/pending outputs may be partly stubbed: `PopButton` must work; `Request*` store pending output and the worker applies it (wheel mode, `setJogLeds`, `setLeds`).
- Construct the driver with `MakeTransport()` (`plugin/src/transport_factory.h`, from `19-oracle-device-windows.md` task 3) so the fake device works in test builds. Every log line pushed to the log queue is also `Trace("log %s", line)`.
- Do not edit `main.cpp`. Do not touch `src/speed_editor/`.

**Verify**
```bash
tools/check && out=$(SPEED_REAPER_FAKE_DEVICE=oracle/fake/wheel_turn.txt ./build/plugin/hid_probe --seconds 3) && echo "$out" | grep -q 'log: connected' && echo "$out" | grep -Eq 'wheel: sum=[1-9]'
```

- [x] Done

### 6. Use `HidWheelSource` in the plugin

- In `plugin/src/main.cpp`: create `static HidWheelSource g_hid`; the engine source becomes `SummedWheelSource(g_hid, g_demo_source)`; `g_hid.Start()` after registration succeeds; `g_hid.Stop()` in the unload path (`rec == nullptr`), before pointers are cleared.
- In `onTimer()`: drain `g_hid.PopLog()` and print each line with `ShowConsoleMsg("Speed Reaper: <line>\n")`. Drain `PopButton()` and discard (milestone 2 uses it).
- Update the ext_name string to `"Speed Reaper"`. Keep the three demo actions.
- Add a `REAPERAPI_WANT_` line for every new REAPER function used (`ShowConsoleMsg`). No other behavior changes; no new actions.

**Verify**
```bash
ORACLE_FULL=1 tools/check
```

- [x] Done

### 7. Oracle scenario: `hid_wheel` (fake device end-to-end)

- Add `oracle/fake/wheel_unplug.txt`: wheel turn, `unplug` at 1500 ms, `plug` at 3000 ms, second wheel turn after 3200 ms.
- Add `oracle/scenarios/hid_wheel.env` (`export SPEED_REAPER_FAKE_DEVICE=oracle/fake/wheel_unplug.txt`) and `oracle/scenarios/hid_wheel.lua`: edit cursor at 10 s, stopped; sample the cursor for 6 s; checks: cursor rose during the first turn (> 0.2 s) with max step < 0.05 s (no stutter); trace has `log connected`, then `log disconnected`, then `log connected` again, in that order; cursor rose again after replug; REAPER stays responsive (the scenario itself reaching `finish()` proves it).
- If a check fails because of a plugin bug, fix the bug (smallest change). Do not change `wheel_unplug.txt` timings to hide a failure.
- Add the scenario to the default `tools/reaper-oracle` run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario hid_wheel
```

- [ ] Done

### 8. SMOKE-TEST.md: "M1 — wheel drives the playhead"

- Append a section `## M1 — wheel drives the playhead` to `SMOKE-TEST.md`, same style as the hello-world section (numbered steps, **Expected** per step):
  1. Prereqs: Resolve and `speedcli` closed; udev rule from Prerequisite 3.
  2. `hid_probe --seconds 10` with the device: expect `log: connected (serial …)`; turn the wheel → `wheel: sum` changes sign with direction; run with device unplugged → `log: device not found`.
  3. Build + install `.so` (reuse the hello-world Install steps by reference); start REAPER with device plugged in: REAPER console shows `Speed Reaper: connected (serial …)`.
  4. Turn the wheel (stopped transport): edit cursor glides as smoothly as the "jog demo" action; release → coasts to a stop in ~0.2 s.
  5. Unplug mid-session: console shows `disconnected`, REAPER stays responsive; replug → `connected` again **without restarting REAPER**.
  6. Restart REAPER with device attached: reconnects.
  7. **Observation (record answers in `docs/notes/m1-hid-observations.md`):** while REAPER holds the device, does pressing any Speed Editor button type characters or trigger REAPER shortcuts (OS-level HID keyboard leak)? Does `speedcli listen` still work while REAPER holds it?
- Add a line at the top of the section: "Automated equivalent: `tools/reaper-oracle --layer reaper --scenario hid_wheel`; this hand test only adds real hardware."
- Do not change other sections.

**Verify**
```bash
grep -q '^## M1 — wheel drives the playhead' SMOKE-TEST.md && grep -q 'hid_probe' SMOKE-TEST.md
```

- [ ] Done

### 9. (human) Run the M1 hand test

- [!] Done
  BLOCKED: precondition — `ORACLE_FULL=1 tools/check` is green (do not hand-test a red oracle). Human then runs SMOKE-TEST.md "M1" top to bottom with the device and records the step-7 observations in `docs/notes/m1-hid-observations.md`. Pass bar: playhead moves with the real wheel with no stutter; unplug/replug is clean with no REAPER restart; keyboard-leak question answered. Then write `vibes/21-m2-buttons-modes.md` tasks as needed and tick this box.
