# Ralph record: 21-m2-buttons-modes

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/check` includes `tools/windows-oracle --layer static`, whose DLL comes from `nix build .#reaper-plugin-windows` — that builds from **committed** git sources. Commit new files before running it (untracked files → "Cannot find source file").
- SWELL (`swell-types.h`) defines `min`/`max` as function-like macros. Any NEW `<vector>`/`<string>`/etc. std include needed by a plugin header must be added to the pre-SDK std-include block at the top of `plugin/src/main.cpp` (existing pattern + comment), or the libstdc++ header's `std::max` breaks under -Werror.
- `tools/check` needs `nix develop` (cmake/g++ not on plain PATH); bgrun jobs: check1/2/3, win, winbuild2, hostorc.
- Plugin `Trace` **appends** to `SPEED_REAPER_TRACE`. tools/reaper-oracle now wipes `trace_<scenario>.txt` before every scenario run (all layers); if you add a scenario, count-based trace checks (e.g. `cmd 40044` once) break on the 2nd run if the file is stale.


## Task: 1. `DiffHeldKeys` (pure) and use it in the worker

### Attempt 1 — 2026-10-12T07:11:23Z — budget 1800s

- Facts: tree clean (no partial work). Design §6: `DiffHeldKeys(const vector<uint16_t>& prev, const vector<uint16_t>& cur, vector<ButtonChange>* out)`, releases first (prev order) then presses (cur order). Created `plugin/src/button_diff.{h,cpp}`, added `button_diff.cpp` to `PLUGIN_PURE_SOURCES` (plugin/CMakeLists.txt line ~48); moved `ButtonChange` to button_diff.h, hid_wheel_source.h includes it; removed private helper + `<algorithm>` from hid_wheel_source.cpp. Test at `plugin/test/button_diff_test.cpp` (tests glob `*_test.cpp`, link PLUGIN_PURE_SOURCES, keycodes from `src/speed_editor/key_map.h`: JOG=0x1D, SCRL=0x1E, SHTL=0x1C, STOP/PLAY=0x3C).
- Dead ends: first `tools/check button_diff` failed compiling `main.cpp`: `stl_bvector.h:1740 std::max(size(), __n)` error under -Werror. Root cause: SWELL (`swell-types.h:128`) defines `max`/`min` as function-like macros; the old main.cpp TU never included `<vector>`, but button_diff.h (via hid_wheel_source.h) pulled it in *after* `reaper_plugin.h`, so `std::max` in the libstdc++ header got macro-mangled. Fix (repo's established pattern, main.cpp top comment): added `#include <vector>` to the pre-SDK std-include block in main.cpp (task 1 forbids the Router, not this one-line include; comment updated).
- Facts: check2: build + `button_diff_test` PASS, but `tools/check` rc=1 in the next step `tools/windows-oracle --layer static` (no output). On a clean tree that step passes instantly (cached nix store DLL); with our source change it must re-run `nix build .#reaper-plugin-windows` (stderr discarded by the script) — likely that rebuild fails or is slow. Re-running it as bgrun job `win` to see the real error.
- Facts: real cause of the windows-oracle failure: `nix build .#reaper-plugin-windows` builds from committed sources; our new files were untracked → "Cannot find source file: button_diff.cpp". Committed (commit on main). After commit: winbuild2 PASS, `tools/reaper-oracle --layer host` PASS, `button_diff_test` PASS.
- Done: check3 (full `tools/check button_diff`) rc=0. Line 20 ticked. Router not written, per task.

Result: exit 0; done; log: logs/21-m2-buttons-modes-L20-a1-1791789083118.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
?? vibes/.ralph/21-m2-buttons-modes.md
```

## Task: 2. `Router` and `ModeGatedSource` (pure)

### Attempt 2 — 2026-10-12T07:19:06Z — budget 1800s

- Facts: no partial work from attempt 1 (tree clean except this record). Design §6 (docs/plugin-design-m1-m3.md:92-103) gives all signatures. Keycodes in `src/speed_editor/key_map.h` (JOG 0x1D, SCRL 0x1E, SHTL 0x1C, STOP/PLAY 0x3C). `IWheelSource`/`WheelSample` in `jog_engine.h` (already in PLUGIN_PURE_SOURCES). Test files glob `*_test.cpp` and include `${REPO_ROOT}/src`, so `#include "speed_editor/key_map.h"` works in tests.
- Facts: created `plugin/src/wheel_mode.h` (ProtocolMode Jog=0/Scrl=2/Shtl=3), `plugin/src/action_sink.h` (IActionSink: RunCommand/ScrollView), `plugin/src/router.h` (Router + ModeGatedSource, kScrollUnitsPerStep=20, initial mode Jog, press rules, SHTL no-op, STOP/PLAY→40044, TakeModeChange once per real change), `plugin/src/router.cpp`, `plugin/test/router_test.cpp`; added `router.cpp` to PLUGIN_PURE_SOURCES. main.cpp untouched (router.h only pulls <cstdint>/<vector>, already pre-SDK-included).
- Dead ends: first `tools/check router` (job check21) failed only in `router_test`: ModeGatedSource negative-turn expectation ignored the scroll remainder 5 left by the previous 45-unit turn (5−40=−35 → 1 step, not 2). Fixed the test to use −45 (5−45=−40 → exactly 2 steps). Router logic itself was correct.
- Dead ends: real Router bug found by the test: negative accumulation used `while (remainder < -k)` which understeps exact multiples (remainder −40 gave 1 step, not 2). Fixed to `<=` in `router.cpp` OnWheel.
- Done: check23 (`tools/check router`) rc=0 — build clean under -Werror, `router_test` 1/1 pass, Windows oracle static PASS, host oracle PASS. Line 33 of vibes/21-m2-buttons-modes.md ticked. main.cpp untouched, per task.

Result: exit 0; done; log: logs/21-m2-buttons-modes-L33-a2-1791789546710.log

### Done — 2026-10-12 — 2 attempt(s)
Files:
```
M vibes/.ralph/21-m2-buttons-modes.md
```

## Task: 3. Wire the router into the plugin

### Attempt 3 — 2026-10-12T07:23:48Z — budget 1800s

- Facts: tree clean (tasks 1–2 committed). Design §6 + oracle §2 confirm: trace lines `mode <Jog|Scrl|Shtl>`, `led jog=<bits> main=<hex>`, `cmd <n>`. SDK has `Main_OnCommand(int,int)` and `CSurf_OnScroll(int xdir,int ydir)` in vendor/reaper-sdk; both names in oracle/reaper_api_names.txt (plugin_host stubs them). Default oracle reaper suite is load,jog_demo,hid_wheel (no buttons scenario yet — task 4 not done). hid_wheel fake scripts have no key events → no mode change, wheel stays pass-through (no regression risk). Jog LED report 0x04 bits: 0=JOG,1=SHTL,2=SCRL → Jog mask 0x1, Scrl mask 0x4.
- Facts: main.cpp edits done: `ReaperActionSink` (RunCommand→`Trace("cmd %d")`+`Main_OnCommand(cmd,0)`, ScrollView→`CSurf_OnScroll(steps,0)`), `g_router`/`g_gated_source`/`SummedWheelSource(g_gated_source, g_demo_source)`, helper `ApplyModeToHid` (traces mode + led, RequestWheelMode + RequestJogLeds matching LED only), onTimer: PopButton→`g_router.OnButton`, `TakeModeChange`→`ApplyModeToHid`, `Connected()` false→true edge→`ApplyModeToHid(current)` via `g_was_connected`. Added `REAPERAPI_WANT_Main_OnCommand`/`_CSurf_OnScroll`. No new std includes needed (router.h's <vector> already pre-SDK-included).
- Facts: plugin_host smoke with `SPEED_REAPER_FAKE_DEVICE` (script /tmp/smoke_buttons.txt): on connect trace shows `mode Jog` + `led jog=1 main=0` (reconnect edge works); SCRL down → `mode Scrl` + `led jog=4 main=0`; STOP/PLAY down → `cmd 40044` exactly once. Host ticks are ~µs-apart, so fake-device script events >~1 ms don't get delivered in a host run — use all-`at 0` events for host-level button tests; timed button flows need the reaper layer (task 4). Note: buttons queued between two timer callbacks collapse to the final mode in one `TakeModeChange` (mode_ read at pop time) — correct by design.
- Done: build3 (build+6 unit tests) rc=0; committed main.cpp (7a05739); checkfull (`ORACLE_FULL=1 tools/check`) rc=0 — WINDOWS ORACLE PASS static, ORACLE PASS all/load,jog_demo,hid_wheel. Line 47 ticked.

Result: exit 0; done; log: logs/21-m2-buttons-modes-L47-a3-1791789828702.log

### Done — 2026-10-12 — 3 attempt(s)
Files:
```
M vibes/.ralph/21-m2-buttons-modes.md
```

## Task: 4. Oracle scenario: `buttons`

### Attempt 4 — 2026-10-12T07:30:25Z — budget 1800s
- Facts: tree clean. Fake script grammar (fake_transport.cpp): `at <ms> key <hex> down|up` (emits full held set as report 0x04), `wheel <delta>`, `unplug`, `plug`; clock = steady time since first successful Open; script plays once. Worker reconnect = 1 s open retry after unplug (hid_wheel_source.cpp step 1). Trace lines are `<ms> <msg>`: `mode Jog|Scrl|Shtl`, `led jog=<hex> main=<hex>` (Jog bit 1, Shtl 2, Scrl 4), `cmd 40044`, `log connected (serial FAKE0001)`, `log disconnected`. Scenario trace file = `build/oracle/trace_<scenario>.txt`; env file sourced by harness; Lua starts ~40 ms after plugin load (offset vs script clock). Default reaper suite = `REAPER_SCENARIOS=(load jog_demo hid_wheel)` in tools/reaper-oracle (line ~206) — adding `buttons` there = "default oracle run".
- Facts: created `oracle/fake/buttons.txt` (JOG tap 300/350; JOG turn 600–912 (40×600 @8 ms); SCRL tap 1200/1250; SCRL turn 1500–1812; SHTL tap 2100/2150; STOP/PLAY tap 2400/2450; unplug 2700; plug 3600; turn 3 4800–5112), `oracle/scenarios/buttons.env`, `buttons.lua` (8 s sampling; windows 1100/1450–1950/≥4700; trace state machine).
- Facts: `tools/reaper-oracle --layer reaper --scenario buttons` PASS first try, all 12 checks non-vacuous: rise1=0.326 s (JOG turn), dev2=0.0000 (SCRL turn), dev3=0.0000 (after replug, still Scrl), GetPlayState 0→1, trace = `mode Jog`@190 → `mode Scrl`@1217 (led jog=1/jog=4, no Shtl), `cmd 40044` once, `led jog=4` re-traced @3717 after the `log disconnected`@2700/reconnect@3700. No plugin bug found (button semantics were verified host-level in attempt 3). Added `buttons` to `REAPER_SCENARIOS` default suite in tools/reaper-oracle (+ comments).
- Dead ends: first full default run (`buttonsfull`) FAILED `buttons: trace: cmd 40044 exactly once` (count=2) — not a plugin bug: in `--layer all` mode `run_reaper` never wiped `trace_$SCENARIO.txt` (only `--layer reaper` did), so Trace (append) piled stale lines from the earlier standalone run; every 2nd default run would fail. Fixed in tools/reaper-oracle: `rm -f trace_$SCENARIO.txt` unconditionally at scenario start (minimal change; no timing edits).
- Done: `buttonsfull2` full default run `ORACLE PASS all/load,jog_demo,hid_wheel,buttons`; verify command re-run again = PASS (deterministic with the trace-wipe fix). Line 60 ticked; commit.

Result: exit 0; done; log: logs/21-m2-buttons-modes-L60-a4-1791790225087.log

### Done — 2026-10-12 — 4 attempt(s)
Files:
```
M vibes/.ralph/21-m2-buttons-modes.md
```
