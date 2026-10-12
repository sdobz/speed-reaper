# Ralph record: 20-m1-wheel-playhead

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).


## Task: 1. Rename the plugin's `WheelEvent` to `WheelSample`

### Attempt 1 — 2026-10-12T02:45:48Z — budget 600s
- Facts:
  - Tree was clean at start; no partial work from earlier attempts.
  - `WheelEvent` occurrences in plugin: `plugin/src/jog_engine.h` (struct def + 2 signatures), `plugin/src/jog_engine.cpp` (2 uses), `plugin/test/jog_engine_test.cpp` (1 use). Renamed all via `sed s/WheelEvent/WheelSample/g` on exactly those three files.
  - `src/speed_editor/event_types.h` also defines its own `WheelEvent` — left untouched per task instructions; the verify grep only scans `plugin/src` and `plugin/test`, so it doesn't interfere.
  - `main.cpp`/`trace.*` don't reference the struct, so nothing else in the plugin needed changes.
  - `tools/check jog_engine` passes (build + jog_engine_test + oracle PASS); `grep -rn WheelEvent plugin/src plugin/test` finds nothing. Full verify command exits 0.
- Dead ends:
  - None.
- Next:
  - Task complete; checkbox ticked. Move on to the next task in the file.

Result: exit 0; done; 4 files changed, 7 insertions(+), 7 deletions(-); log: logs/20-m1-wheel-playhead-L26-a1-1791773148918.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M plugin/src/jog_engine.cpp
 M plugin/src/jog_engine.h
 M plugin/test/jog_engine_test.cpp
 M vibes/20-m1-wheel-playhead.md
?? vibes/.ralph/20-m1-wheel-playhead.md
```

## Task: 2. Fix the demo wiring: `SummedWheelSource`

### Attempt 2 — 2026-10-12T02:46:37Z — budget 600s
- Facts:
  - At start, only the record file was dirty; `main.cpp` already had the demo action driving `g_demo_source` directly, but the engine was built on `g_demo_source` alone and `JogEngine` still owned `demo_`/`ToggleDemo()`/etc.
  - Added `NullWheelSource` + `SummedWheelSource` (header-only, refs to both sources) to `plugin/src/jog_engine.h`; removed `demo_`, `demo_on_`, `ToggleDemo()`, `DemoActive()`, `Demo()` from `JogEngine`. No other references existed (`grep -rn "ToggleDemo|DemoActive|Demo()" plugin/` clean).
  - `main.cpp`: `NullWheelSource g_null;` + `SummedWheelSource g_wheel_source(g_demo_source, g_null);`; engine built on `g_wheel_source`.
  - Test: sums two `FixedSource`s (7 + -3 = 4); triggered `DemoWheelSource` through `SummedWheelSource(demo, null)` into engine moves `SumSink` forward, then settles.
  - `tools/check jog_engine` exits 0 (test + oracle PASS).
- Dead ends:
  - `SummedWheelSource src2(demo, NullWheelSource())` — cannot bind rvalue to `IWheelSource&` param; use a named local.
  - Settle check right after the turn: the low-pass velocity (tau=50 ms) keeps drifting for ~800 ms, so the sink moves >1e-9 within a short window. Fix: coast 260 ticks (turn 1.25 s + 800 ms ≈ 16 tau) before measuring `settled2`, then CHECK_NEAR over 10 ticks — same pattern as the pre-existing test.
- Next:
  - Task complete; checkbox ticked.

Result: exit 0; done; log: logs/20-m1-wheel-playhead-L44-a2-1791773197941.log

### Done — 2026-10-12 — 2 attempt(s)
Files:
```
M vibes/.ralph/20-m1-wheel-playhead.md
```

## Task: 3. Wire the driver and hidapi into the plugin build (stub `HidWheelSource`)

### Attempt 3 — 2026-10-12T02:49:28Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Tree clean at start (only the record file modified). No partial work from earlier attempts.
- Design §4 interface lives in `docs/plugin-design-m1-m3.md` lines 45–83; `IWheelSource` is in `plugin/src/jog_engine.h` (virtual `~IWheelSource()`, pure `Tick(WheelSample*)`).
- `src/speed_editor/hid_device.h` = `class SpeedEditorHID` (includes `<hidapi/hidapi.h>`); driver already compiles with -Wall -Wextra via `src/Makefile` (which links `hidapi-hidraw` via pkg-config), so that pkg-config module exists in the nix env.
- `plugin/test/CMakeLists.txt` globs `*_test.cpp`, each test executable links `${PLUGIN_PURE_SOURCES}` — so appending the two new .cpp to `PLUGIN_PURE_SOURCES` (before `add_library`) covers plugin target and tests at once; tests additionally need `PkgConfig::HIDAPI` + include dirs.
- `tools/check` auto-enters `nix develop` when cmake is missing; builds with `-DCMAKE_CXX_FLAGS=-Werror -DSPEED_REAPER_FAKE_DEVICE=ON`.
- Decision: header cannot hold a *value* `SpeedEditorHID dev_` (type incomplete in .h, and hid_device.h may only be included in the .cpp) → forward-declare `class SpeedEditorHID;` and hold `std::unique_ptr<SpeedEditorHID> dev_;` (member still named `dev_`; destructor defined in .cpp where the type is complete).
- Header declares the full §2 private state (thread, running flag, atomic delta/abs/connected, mutex+deque log/buttons, mutex+PendingOutput) so task 5 replaces only the .cpp.
- CMake: moved `REPO_ROOT` computation to the top; `PkgConfig::HIDAPI` must be created before `add_subdirectory(test)` so test targets can link it — the test/ guard block now sits after the `if(UNIX AND NOT APPLE)` block.
- Result: full Verify green on first build (`tools/check` + readelf → VERIFY_OK; log `vibes/.ralph/logs/task3-attempt3.log`). `.so` has `NEEDED libhidapi-hidraw.so.0`; both unit tests pass; oracle host layer passes. hidapi pkg module in nix env is `hidapi-0.15.0`.
Next:
- Task 4 (`hid_probe`) — links the same PLUGIN_PURE_SOURCES + PkgConfig::HIDAPI; the stub already prints nothing, so the probe just needs its 8 ms Tick/PopLog loop and the `wheel: sum=` line.

Result: exit 0; done; log: logs/20-m1-wheel-playhead-L58-a3-1791773368224.log

### Done — 2026-10-12 — 3 attempt(s)
Files:
```
M vibes/.ralph/20-m1-wheel-playhead.md
```

## Task: 4. Probe tool `hid_probe`

### Attempt 4 — 2026-10-12T02:56:10Z — budget 600s
- Facts:
  - Design §5 spec: `hid_probe [--seconds N] [--buttons]`, 8 ms Tick/PopLog/(PopButton) loop, running wheel sum printed once per second, exit 0 always; prefixes `log: `, `wheel: sum=<n>`, `button: <key hex> down|up`.
  - `plugin/CMakeLists.txt`: plugin target = `src/main.cpp` + `PLUGIN_PURE_SOURCES`; on UNIX PLUGIN_PURE_SOURCES already includes `hid_device.cpp` + `hid_wheel_source.cpp`, and `PkgConfig::HIDAPI` (hidapi-hidraw) is in scope inside the `if(UNIX AND NOT APPLE)` block.
  - Wrote `plugin/tools/hid_probe.cpp` (no REAPER, 8 ms loop, per-second `wheel: sum=` line, arg errors exit 2) and added `add_executable(hid_probe tools/hid_probe.cpp ${PLUGIN_PURE_SOURCES})` inside the UNIX block with same includes + `PkgConfig::HIDAPI`; output lands in `build/plugin/` by default.
  - `tools/check` running in background, log: `vibes/.ralph/logs/task4-check.log`.
- Dead ends: (none yet)
- Verified: `tools/check` green (2/2 ctest, oracle host PASS); `./build/plugin/hid_probe --seconds 2` prints only `wheel: sum=0` (twice, one per second) and exits 0; full Verify command exits 0. Box ticked.

Result: exit 0; done; log: logs/20-m1-wheel-playhead-L71-a4-1791773770714.log

### Done — 2026-10-12 — 4 attempt(s)
Files:
```
M vibes/.ralph/20-m1-wheel-playhead.md
```

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 5 attempt(s), last: exit 0, verify-failed
Log: logs/20-m1-wheel-playhead-L71-replan-1791774177159.log

## Task: 5. Implement the `HidWheelSource` worker thread

### Attempt 5 — 2026-10-12T02:59:48Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Tree clean at start (only the record file dirty). No partial work from earlier attempts.
  - `vibes/19-oracle-device-windows.md` does not exist in this repo (vibes/ = 00, 05, 10, 18, 20, 21, 22, 23, 30); no git branch/commit ever added it or the fake-transport files. Task 3 of that file is therefore unscheduled/undone.
  - Missing artifacts the task's Verify needs: `plugin/src/transport_factory.h` (grep for `MakeTransport` matches only this checklist line), `src/speed_editor/hid_transport.h` + `fake_transport.{h,cpp}`, `oracle/fake/` directory (no `wheel_turn.txt`), and the `SPEED_REAPER_FAKE_DEVICE` CMake option (CMakeLists never `option()`s it; `-DSPEED_REAPER_FAKE_DEVICE=ON` in tools/check is silently unused — CMake tolerates unknown -D vars, which is why task 3/4 stayed green).
  - The design for the seam IS settled: `docs/plugin-oracle-design.md` §6 specifies `IHidTransport` in `src/speed_editor/hid_transport.h`, `FakeTransport` playing `at <ms> wheel|key|unplug|plug` scripts selected by env `SPEED_REAPER_FAKE_DEVICE=<script>` only when built with `-DSPEED_REAPER_FAKE_DEVICE=ON`, scripts in `oracle/fake/*.txt`. That's a whole task of its own (auth-handshake simulation included), scoped under 19-…, and this task's block forbids touching `src/speed_editor/`.
  - `HidWheelSource` stub is intact per task 3: header (`hid_wheel_source.h`) already declares full §2 state (thread, atomics, mutex+deque log/buttons, `PendingOutput`), so the worker-loop implementation itself is the remaining work once the transport exists. `SpeedEditorHID` API: `open/close/authenticate/setWheelMode/readEvent/setLeds/setJogLeds/allLedsOff` (src/speed_editor/hid_device.h).
- Dead ends:
  - `find`/`grep -rn 'MakeTransport'` across the repo: the only hit is the checklist line itself — nothing to continue, no hidden WIP.
- Next:
  - Planner: either (a) add a task implementing `docs/plugin-oracle-design.md` §6 (transport seam + FakeTransport + CMake option + `oracle/fake/wheel_turn.txt`, porting 19-… task 3 since the file is absent here), then re-run this task; or (b) amend this task to lift the "do not touch `src/speed_editor/`" constraint and fold §6 in. Either way the worker-loop work in `hid_wheel_source.cpp` can proceed once `MakeTransport()` exists.
  - This attempt is BLOCKED (line 85 marked `[!]`); no code changes made.

Verify failed (exit 1):
```
Welcome to the speed-reaper development environment!
  g++      g++ (GCC) 16.2.0
  cmake    cmake version 4.4.3
  reaper   ...
SDK include roots: -Ivendor/reaper-sdk/sdk -Ivendor/WDL/WDL (see docs/reaper-dev-env.md §2)
Build CLI (task 3): make -C src                → build/speedcli
Build CLI (task 5): nix build .#packages.x86_64-linux.speedcli              → .../bin/speedcli
                   nix build .#packages.x86_64-pc-windows-gnu.speedcli → .../bin/speedcli.exe
Build REAPER plugin (task 6): cmake -S plugin -B build/plugin -G Ninja && cmake --build build/plugin
  → build/plugin/reaper_speed_reaper.so (install: see SMOKE-TEST.md, 'REAPER plugin' section)
Build REAPER plugin for Windows (task 7): nix build .#reaper-plugin-windows → .../reaper/reaper_speed_reaper.dll
  Cross-platform matrix + gotchas: CROSS-PLATFORM.md
Oracle: tools/reaper-oracle (headless REAPER smoke + trace checks — vibes/18-oracle-linux.md)
[0/2] Re-checking globbed directories...
ninja: no work to do.
Test project /home/vkhougaz/projects/speed-reaper/build/plugin
    Start 1: jog_engine_test
1/2 Test #1: jog_engine_test ..................   Passed    0.00 sec
    Start 2: trace_test
2/2 Test #2: trace_test .......................   Passed    0.00 sec

100% tests passed out of 2

Total Test time (real) =   0.01 sec
[0/2] Re-checking globbed directories...
ninja: no work to do.
PASS=True REASONS=[]
ORACLE PASS host
warning: Git tree '/home/vkhougaz/projects/speed-reaper' is dirty
```

Result: exit 0; verify-failed; 2 files changed, 52 insertions(+), 2 deletions(-); log: logs/20-m1-wheel-playhead-L85-a5-1791773988106.log

### Attempt 1 — 2026-10-12T04:22:01Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - The attempt-5 blocker is gone: `vibes/19-…` task 3 landed meanwhile (commits `a86161b`/`1921b25`) — `plugin/src/transport_factory.{h,cpp}`, `src/speed_editor/fake_transport.{h,cpp}` + `hidapi_transport.cpp`, `oracle/fake/wheel_turn.txt`, and the `SPEED_REAPER_FAKE_DEVICE` CMake option (default OFF) all exist; `tools/check` builds with `-DSPEED_REAPER_FAKE_DEVICE=ON`.
  - `SpeedEditorHID` takes `std::unique_ptr<IHidTransport>` in its ctor (default nullptr = real), so `std::make_unique<SpeedEditorHID>(MakeTransport())` is the whole seam. FakeTransport serial is `FAKE0001`; its wheel input reports echo the mode set via `setWheelMode` (wheel_turn script → mode 0 → relative deltas).
  - Implementation: `hid_wheel_source.cpp` worker loop = design §4 steps 1–5 (1 s open retry via `SleepSliced` ≤100 ms, `device not found` logged once until open succeeds, `auth failed` → close → retry, init `setWheelMode(0)`+`setJogLeds(true,false,false)` → `connected (serial …)`, event loop with `readEvent(ev, 20)`, disconnect detected via `getState()==State::Disconnected` → `disconnected`, worker exits with `close()`). `DiffHeldKeys` is a file-local anonymous-namespace helper (releases in prev order, then presses in cur order) pending M2's `button_diff.h`. Pending outputs applied between reads: 18 LED bits → three 6-bit bytes for `setLeds`.
  - Added private methods `WorkerLoop/PushLog/ApplyPending/SleepSliced` to `hid_wheel_source.h` (worker-thread-only). `PushLog` also `Trace("log %s", line)`.
  - `Stop()` = `running_.exchange(false)` + join; worker wakes within one 100 ms slice, so join is well under 1 s even with no device. `main.cpp` untouched, `src/speed_editor/` untouched.
- Dead ends:
  - `std::min(left, std::chrono::milliseconds(100))` with `left` in steady_clock nanosecond duration doesn't compile (deduction failure) — cast `left` to `milliseconds` first.
- Next:
  - Task complete; checkbox ticked, full Verify (tools/check + fake-device probe greps) exits 0 (`VERIFY_OK` in vibes/.ralph/logs/task5-verify.log).

Result: exit 0; done; log: logs/20-m1-wheel-playhead-L85-a1-1791778921838.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M vibes/.ralph/20-m1-wheel-playhead.md
```

## Task: 6. Use `HidWheelSource` in the plugin

### Attempt 2 — 2026-10-12T04:28:55Z — budget 600s
- Facts:
  - `main.cpp` already had `SummedWheelSource g_wheel_source(g_demo_source, g_null)`; swapped `g_null` → `HidWheelSource g_hid` (order: `g_hid` first, then `g_demo_source`), added `REAPERAPI_WANT_ShowConsoleMsg`, `#include "hid_wheel_source.h"`, ext_name → `"Speed Reaper"`, `g_hid.Start()` after timer registration, `g_hid.Stop()` first in the `rec == nullptr` path.
  - `onTimer()` drains `PopLog` → `ShowConsoleMsg("Speed Reaper: <line>\n")` and discards `PopButton`, before the engine-null early return.
  - No test/tool references the old `"Speed Reaper (hello world)"` ext_name, so the rename is safe.
  - `ButtonChange` is defined in `hid_wheel_source.h:31`; `PopButton(ButtonChange*)` signature confirmed.
- Next:
  - `ORACLE_FULL=1 tools/check` (background, log at vibes/.ralph/logs/task6-verify.log); tick line 99 if exit 0.

### Attempt 3 — 2026-10-12T~05:10Z — budget 1800s
- Facts:
  - All task-6 code was already complete in the working tree (uncommitted): diff covers g_hid, SummedWheelSource order, Start/Stop, onTimer drains, ext_name, REAPERAPI_WANT_ShowConsoleMsg. Matches the task bullet-for-bullet.
  - Prior `vibes/.ralph/logs/task6-verify.log` ends with `ORACLE PASS all/load,jog_demo` (but that run was wrapped in bgrun whose wrapper reported exit 127 — so re-running verify directly to get a clean exit code).
  - Re-running verify as bgrun job `t6-verify3` (log `vibes/.ralph/logs/t6-verify3.log`); appends `CHECK_EXIT=$?` at the end.
  - `bgwait t6-verify3`: build clean, 4/4 ctest pass, `ORACLE PASS all/load,jog_demo`, `CHECK_EXIT=0`. Line 99 ticked to `- [x]`; done.

### Attempt 1 — 2026-10-12T04:48:37Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
