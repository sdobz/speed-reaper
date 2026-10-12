# Ralph record: 19-oracle-device-windows

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/check` builds the plugin with `-DSPEED_REAPER_FAKE_DEVICE=ON` already; builds need the nix shell (`tools/check` self-launches `nix develop`).
- Task 1 done: `src/speed_editor/hid_transport.h` (`IHidTransport`), `hidapi_transport.{h,cpp}` (`HidapiTransport`); `SpeedEditorHID(std::unique_ptr<IHidTransport> = nullptr)`. Stderr diagnostics + state machine stay in the driver; transport is a thin wrapper (returns hidapi codes).


## Task: 1. `IHidTransport` seam under the driver

### Attempt 1 — 2026-10-12T03:31:39Z — budget 600s

- Facts: design §6 interface is `Open/Close/Read/Write/SendFeature/GetFeature/Serial` (PascalCase). Build files touching the driver: only `src/Makefile` (SRCS) and `plugin/CMakeLists.txt` (Linux `PLUGIN_PURE_SOURCES`). `SPEED_EDITOR_VID/PID` live in `src/speed_editor/key_map.h`. HidapiTransport keeps enumeration + serial storage; driver keeps a `bool opened` flag (replaces the old `dev != nullptr` guards) and stderr diagnostics — behavior identical. `buf[0]=reportId` for feature reads moved into `HidapiTransport::GetFeature`.
- Facts: `src/cli/speedcli.cpp` `cmdScan` calls `hid_enumerate`/`hid_free_enumeration` directly (previously pulled in via `hid_device.h`); added `#include <hidapi/hidapi.h>` there — CLI keeps direct enumeration, seam only covers the driver.
- Facts: builds need the nix shell (`tools/check` self-launches `nix develop`); verify passed: tools/check green (plugin -Werror, 2 unit tests, ORACLE PASS host), `speedcli scan` exits 1 (no device, ≤1 allowed). Line 21 ticked.

Result: exit 0; done; 2 files changed, 5 insertions(+), 4 deletions(-); log: logs/19-oracle-device-windows-L21-a1-1791775899714.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M vibes/.ralph/19-oracle-device-windows.md
 M vibes/20-m1-wheel-playhead.md
```

## Task: 2. `FakeTransport` and its script parser (pure)

### Attempt 2 — 2026-10-12T03:37:22Z — budget 600s
- Facts: all files written: `src/speed_editor/auth.h` (free functions `speed_editor_auth::{rol8,rol8n,calculateResponse}` + tables; `SpeedEditorHID` static auth helpers removed, `authenticate()` calls `speed_editor_auth::calculateResponse`), `fake_transport.{h,cpp}`, `plugin/test/fake_transport_test.cpp`; `fake_transport.cpp` added to the Linux `PLUGIN_PURE_SOURCES` block in `plugin/CMakeLists.txt`. `src/Makefile` SRCS untouched (CLI doesn't need the fake).
- Facts: design decisions — script plays once from the *first* successful Open (reconnect continues the same timeline; wheel mode + held keys + auth re-arm on each Open, like re-enumeration); Read blocks until next due event or timeout; input reports gated on auth Accepted (0 otherwise); GetFeature 0x06 returns [06,01,challenge LE] / [06,02] / [06,04 accepted] / [06,05 rejected]; `at <ms> unplug|plug` and stray `plug` (no unplug) is a no-op consume.
- Dead ends: first background launch (`g++ … && setsid nohup tools/check … &` inside one && chain) died with no log — the harness kills the chain when the foreground echo exits; relaunch as `setsid bash -c '…' </dev/null & disown` works.
- Next: `tools/check fake_transport` running in background, log `vibes/.ralph/logs/task2-build.log` (nix develop → make src → cmake plugin -Werror → ctest -R fake_transport → oracle host). Poll log; if green, tick line 35 of `vibes/19-oracle-device-windows.md`.

Result: timeout after 600s; progress; 4 files changed, 14 insertions(+), 55 deletions(-); log: logs/19-oracle-device-windows-L35-a2-1791776242317.log

### Attempt 3 — 2026-10-12T03:47:57Z — budget 600s
- Facts: attempt 2's files are all present and complete; attempt 2's build (log `vibes/.ralph/logs/task2-build.log`) failed on one compile error: `fake_transport_test.cpp:7: fatal error: fake_transport.h: No such file` — the test used bare includes, but the test targets' include root is `${REPO_ROOT}/src` (convention: `speed_editor/hid_device.h`, see `plugin/src/hid_wheel_source.cpp`). Fixed includes in the test to `speed_editor/fake_transport.h` / `speed_editor/hid_device.h`.
- Facts: verified `SpeedEditorHID` semantics against the test: `readEvent` false on timeout (state unchanged), false + `close()` (→ Disconnected) on Read <0; `authenticate` needs GetFeature replies [06,01,challenge]/[06,02]/[06,04] — FakeTransport's state machine matches.
- Facts: test auto-registers via `file(GLOB *_test.cpp)` in `plugin/test/CMakeLists.txt`; no CMakeLists change needed beyond attempt 2's `PLUGIN_PURE_SOURCES` line.
- Next: `tools/check fake_transport` relaunched (attempt 3), log `vibes/.ralph/logs/task2-verify-a3.log` (ends with `EXIT=<n>`). Poll it; if `EXIT=0`, tick line 35 of `vibes/19-oracle-device-windows.md`.

Result: timeout after 600s; progress; 4 files changed, 22 insertions(+), 55 deletions(-); log: logs/19-oracle-device-windows-L35-a3-1791776877969.log

### Replan (depth 0) — REWRITE → "Fix the fake_transport_test expectation and make `tools/check fake_transport` green"
Trigger: 3 attempt(s), last: timeout after 600s, progress
Why: Both attempts burned the 600s budget on build orchestration; a3's log shows the build now compiles and the only live defect is the test's own expectation — goodScript() has 7 event lines but line 33 checks evs.size() == 8 — while the 14 bad-case failures in that log came from a stale parser revision (a3's build never recompiled fake_transport.cpp, and the on-disk parser sets *err on all 13 bad cases).
Rejected: BUDGET: the approach is fine and the remaining work is one line; a bigger budget with the old 'create the four files' block risks another full orbit re-reading and re-doing finished work. SPLIT: nothing to split — one expectation fix plus the existing Verify. SPIKE: no open design decision; all contracts are settled and the code is written. BLOCK: nothing needs a human.
Log: logs/19-oracle-device-windows-L35-replan-1791777517928.log

## Task: Fix the fake_transport_test expectation and make `tools/check fake_transport` green

### Attempt 1 — 2026-10-12T04:04:47Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: fixed `plugin/test/fake_transport_test.cpp:33` (and the fprintf guard) 8 → 7; confirmed `ParseFakeScript` in `src/speed_editor/fake_transport.cpp` fills `*err` for all 13 bad-script cases (unknown verb, wheel arg count/delta, bad/negative time, bad key state/keycode/range/missing state, unplug/plug extra arg, missing 'at', empty-line/comment skipping).
- Done: `tools/check fake_transport` exits 0 (`fake_transport_test` passed, `ORACLE PASS host`); log at `vibes/.ralph/logs/task2-verify-b.log`. Ticked line 35.

Result: exit 0; done; log: logs/19-oracle-device-windows-L35-a1-1791777887215.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M vibes/.ralph/19-oracle-device-windows.md
```

## Task: 3. Select the fake by env (test builds only)

### Attempt 1 — 2026-10-12T04:08:16Z — budget 600s

- Facts:
  - `hid_wheel_source.cpp` already exists as a stub (milestone 1); it does not construct a transport yet — not my job per the task text.
  - `tools/check` passes `-DSPEED_REAPER_FAKE_DEVICE=ON` inside `-DCMAKE_CXX_FLAGS` (compiler flag, not a CMake cache var) — so the macro comes from CXX_FLAGS in check builds; the new CMake option (default OFF) stays OFF there, no define clash.
  - `FakeTransport::Read` emits no input reports until the 0x06 auth handshake passes, so the factory test uses a `SpeedEditorHID` session (open + authenticate + readEvent), same pattern as `fake_transport_test.cpp`.
  - Added: `plugin/src/transport_factory.{h,cpp}`, `plugin/test/transport_factory_test.cpp`, `oracle/fake/gen_wheel_turn.py` + generated `wheel_turn.txt` (62× `wheel 20` @8 ms from t=300, then 20 deltas 20→1 @25 ms). CMake option + `add_compile_definitions($<$<BOOL:..>:SPEED_REAPER_FAKE_DEVICE>)` at directory scope covers plugin, hid_probe, tests.
  - Factory with env set always returns FakeTransport; unreadable/malformed script → stderr note + empty script.
- Next: run `tools/check` (log: vibes/.ralph/logs/build.log), fix anything red, tick line 55.

Result: timeout after 600s; progress; 2 files changed, 22 insertions(+); log: logs/19-oracle-device-windows-L55-a1-1791778096328.log

### Attempt 2 — 2026-10-12T04:18:54Z — budget 600s
- Facts:
  - Attempt 1's work was complete and consistent: `plugin/src/transport_factory.{h,cpp}`, `plugin/test/transport_factory_test.cpp` (auto-globbed by `plugin/test/CMakeLists.txt`), CMake option + `add_compile_definitions($<$<BOOL:..>:SPEED_REAPER_FAKE_DEVICE>)` at directory scope, factory added to the Linux `PLUGIN_PURE_SOURCES` block, `oracle/fake/gen_wheel_turn.py` + `wheel_turn.txt` (82 lines: 62× `wheel 20` @8 ms from t=300, then deltas 20→1 @25 ms).
  - `tools/check` exit 0: all 4 plugin tests pass (incl. `transport_factory_test`), `ORACLE PASS host`; `test -s oracle/fake/wheel_turn.txt` ok. Build was incremental — attempt 1 had already built before timing out.
- Next: task done, box ticked.
