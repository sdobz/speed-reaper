# Ralph record: 40-cleanup

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/reaper-oracle --layer all`: the `verbs` scenario is STALE (pre-existing since M8, as of 40-cleanup task 2): it expects exactly the 8 bare verb cmd ids (impl wraps RIPL DEL in ripple set/restore 40310/40309 when ripple is off) and `led main=` of only 0x20/0x00 (impl also sets LED_CAM1 0x4000 for selected single-take items). Expect `ORACLE FAIL all/...: verbs` until someone rewrites the scenario or the impl.
- Clean-HEAD baselines via `git worktree` are not usable for the REAPER layer: REAPER needs an absolute `$HOME`, but `tools/reaper-oracle` passes the relative `build/oracle/home` → "Failed to get the home directory" → scenario timeout. Baseline in the main tree instead (e.g. `git stash` + run + `git stash pop`).
- Nix builds (e.g. `nix build .#reaper-plugin-windows`, `src = ./.`) copy the repo via a path dep that IGNORES untracked files in a dirty git tree (the /nix/store/...-source copy lacks them → "fatal error: <newfile>: No such file or directory" while edited tracked files ARE present). `git add` new files before any nix build; the build then re-evaluates to a new source path.
- `tools/reaper-oracle --layer reaper` without `--scenario` runs ONLY the `load` scenario (the full 17-scenario suite is `--layer all`), so the stale `verbs` scenario doesn't affect `--layer reaper` Verify commands.


## Task: 1. Repo hygiene

### Attempt 1 — 2026-10-15T02:21:56Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - `oracle/scenarios/spike_xfade.lua` was ALREADY tracked (committed in 235b32a) — nothing to do there.
  - `.gitignore` already covered `oracle/__pycache__/` (via `__pycache__/`) and `build/oracle/` (via `build/`); added `vibes/.ralph-logs/` + `build/oracle/` explicitly anyway; `git rm -r --cached vibes/.ralph-logs` done.
  - The six probes are not in `REAPER_SCENARIOS` in `tools/reaper-oracle` (only `probe.json` output names match `probe_`). Deleted.
  - Docs citing probes, citation replaced by finding: `docs/notes/m4-shuttle-api.md` (scenario path → "since-deleted one-off scenario"), `docs/plugin-oracle-design.md` (scenario catalogue line → pointer to m4-shuttle-api.md), `docs/notes/m5-observations.md` (stripped probe names, findings stay), `vibes/32-m6-simple-verbs.md` (line 90), `vibes/23-m4-shuttle.md` (line 42). Also stripped two comment citations in `oracle/scenarios/smth_cut.lua` (in-suite scenario, would have been dangling). vibes/23 line 43/48 keep `--scenario spike_rate` as historical command invocations of a checked-off task, not path citations.
- Dead ends:
  - none so far
- Next:
  - DONE: `tools/check` green (job check40-1), full Verify rc=0, line 24 ticked, committed as `40-cleanup: 1. Repo hygiene`.

Result: exit 0; done; log: logs/40-cleanup-L24-a1-1792030916691.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
?? vibes/.ralph/40-cleanup.md
```

## Task: 2. Delete the hello-world demo and the M4 spike code

### Attempt 2 — 2026-10-15T02:26:43Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - Test file is `plugin/test/jog_engine_test.cpp` (task said `plugin/src/…` — wrong path in task).
  - `Master_GetPlayRate`, `CSurf_OnPlayRateChange`, `OnStopButton` are still called by shuttle (`ApplyShuttleRate`, `HardStop` in main.cpp) → keep the 3 `REAPERAPI_WANT_*` lines, only strip "spike" from their comments.
  - `jog_engine.cpp` contains only `DemoWheelSource` methods → delete file + its line in `plugin/CMakeLists.txt` (line 52).
  - Oracle fallout beyond the task text: `oracle/scenarios/load.lua` checks the 3 demo action ids via `NamedCommandLookup` and `run_host()` in `tools/reaper-oracle` passes `--expect-actions speed_reaper_jogdemo,…` to `plugin_host` — both must be updated or the reaper/host layers fail. Plugin registers zero actions after the delete.
  - `load.lua` rewrite: plugin no longer registers actions, so it now checks the trace file (lib.lua `read_trace()`/`find_line()` work in scenarios; `SPEED_REAPER_TRACE` is set per scenario) for `entry result=1` / `register timer ok` / `hid source started`.
  - `SummedWheelSource`/`NullWheelSource`/`JogEngine::NudgeSeconds` had no callers outside main.cpp + jog_engine_test.cpp → all four demo types deleted from `jog_engine.h`; engine constructed directly from `g_gated_source`.
  - `onHookCommand2` + `RegisterAction` helper + `hookcommand2` registration removed entirely (no actions remain to hook).
  - SMOKE-TEST.md: Build/Install/Uninstall + gdk.h note + Windows cross-build moved into numbered Prerequisites 5–7; M1 step 3's "hello-world" pointer updated to "Prerequisites 5–6".
  - Progress: all edits done. `tools/check` green (job check40-2: 14/14 unit tests, host oracle PASS, Windows build PASS). `tools/reaper-oracle --layer reaper` (runs only `load` — the full suite needs `--layer all`) PASSED with the rewritten load.lua. Full suite (`--layer all`, job suite40-2) running as regression check — note it in Next.
  - One dangling pointer fixed along the way: flake.nix dev-shell banner line 177 pointed at the deleted SMOKE-TEST 'REAPER plugin' section → now "Prerequisites 5–6". (docs/reaper-dev-env.md "hello world" hits are about the upstream reaper-sdk-vscode template — left alone.)
- Facts:
  - suite40-2 (`--layer all`): ALL scenarios pass except `verbs` — 2 failing checks: (a) `cmd lines are the 8 verb ids` — trace shows `40310 40006 40309` (verbs.cpp RIPL DEL wraps delete in ripple set/restore when ripple is OFF, which is the fresh-project default) vs the scenario's expected bare 8 ids; (b) `led main=` final value 0x4000 (LED_CAM1 = shared active take 1 on the selected single-take items) vs expected 0x20/0x00. Both live in verbs.cpp/led_state/reaper_project.cpp — untouched by this task. Suspect pre-existing (no `--layer all` run since M8).
  - Baseline check: clean-HEAD worktree at /tmp/sr-clean (job baseline-verbs) runs `--scenario verbs` to prove the failure predates my diff.
  - Baseline worktree at /tmp/sr-clean was environmentally broken (REAPER "Failed to get the home directory, not an absolute path: build/oracle/home" → scenario timeout; worktree removed again). NOT a valid baseline — do not retry the worktree approach; REAPER wants an absolute HOME.
  - Pre-existing verdict instead via code inspection: the 2 failing `verbs` checks depend only on `verbs.cpp` RunRippleDelete (ripple wrap 40310/40006/40309 when ripple off — fresh-project default) and `ReadLedState`/`MainLedMask` (selected single-take items → active_take=1 → LED_CAM1 bit 0x4000; scenario expects only 0x20/0x00). `git diff --stat` on verbs.cpp/reaper_project.cpp/led_state.*/gesture.*/verbs.lua = empty: my diff touches none of it. ⇒ `--layer all` verbs failure predates this task (no full-suite run since M8 landed the LED bits + ripple wrap). The task Verify command runs `--layer reaper` = `load` scenario only, which passes.
- Next:
  - DONE: verify40-2 (exact Verify command) rc=0 — tools/check green (14/14 unit tests, host + Windows oracle PASS), grep clean, `ORACLE PASS reaper/load`. Line 39 of vibes/40-cleanup.md ticked. Job names: check40-2 (tools/check), oracle40-2 (--layer reaper), suite40-2 (--layer all, verbs pre-existing failure), verify40-2 (exact Verify).

Result: exit 0; done; log: logs/40-cleanup-L39-a2-1792031203704.log

### Done — 2026-10-15 — 2 attempt(s)
Files:
```
M vibes/.ralph/40-cleanup.md
```

## Task: 3. One REAPER API list

### Attempt 3 — 2026-10-15T02:42:24Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - Clean tree (only record file modified); no partial work from earlier attempts.
  - Union of the two old lists = 39 unique names (main.cpp had 41 lines incl. 2 dupes: GetCursorPosition, GetToggleCommandState).
  - `reaper_plugin_functions.h` lives at `vendor/reaper-sdk/sdk/` (SYSTEM include dir); guarded by `_REAPER_PLUGIN_FUNCTIONS_H_`, defines globals only when `REAPERAPI_IMPLEMENT`, else externs + `REAPERAPI_LoadAPI` declaration. Safe to include from a shared header.
  - New `plugin/src/reaper_api.h` = MINIMAL + 39 WANT lines (comment = caller) + `#include "reaper_plugin_functions.h"`. main.cpp: `#define REAPERAPI_IMPLEMENT` then include; reaper_project.cpp: plain include. All duplicated lists + "keep in sync" comments deleted.
  - New `tools/check_api_want` (python3): strips comments/string literals (state machine incl. raw strings) from `plugin/src/*.cpp`, whole-word-matches names from `oracle/sdk_api_names.txt`, fails listing missing WANT names. Comment-only mentions (CSurf_OnScroll, CSurf_ScrubAmt, plugin_register) do NOT count — that's why stripping is required. Negative-tested (removing a line → FAIL rc=1).
  - Wired `tools/check_api_want` into `tools/check` right after the nix-develop guard, before the build.
  - check40-3 FAILED at the Windows cross build: nix source copy lacked the untracked `reaper_api.h` (see Learnings). Fixed by `git add plugin/src/reaper_api.h tools/check_api_want`; rebuild win40-3 rc=0.
  - Also fixed before verify: the comment in reaper_project.cpp originally contained the literal `REAPERAPI_WANT_*` — the Verify `rg -c` counts comment occurrences too, so it would have made 2 files match; reworded the comment.
  - DONE: verify40-3 (exact Verify command) rc=0 — tools/check green (check_api_want OK, 14/14 unit tests, WINDOWS ORACLE PASS static, ORACLE PASS host), `rg -c` count = 1 (only reaper_api.h), ORACLE PASS reaper/load. Line 51 ticked.

Verify failed (exit 1):
```
.....   Passed    0.00 sec
      Start  4: hold_nudge_test
 4/14 Test  #4: hold_nudge_test ..................   Passed    0.00 sec
      Start  5: jog_engine_test
 5/14 Test  #5: jog_engine_test ..................   Passed    0.00 sec
      Start  6: led_read_test
 6/14 Test  #6: led_read_test ....................   Passed    0.00 sec
      Start  7: led_state_test
 7/14 Test  #7: led_state_test ...................   Passed    0.00 sec
      Start  8: nudge_test
 8/14 Test  #8: nudge_test .......................   Passed    0.00 sec
      Start  9: router_test
 9/14 Test  #9: router_test ......................   Passed    0.00 sec
      Start 10: shuttle_test
10/14 Test #10: shuttle_test .....................   Passed    0.00 sec
      Start 11: target_test
11/14 Test #11: target_test ......................   Passed    0.00 sec
      Start 12: trace_test
12/14 Test #12: trace_test .......................   Passed    0.01 sec
      Start 13: transport_factory_test
13/14 Test #13: transport_factory_test ...........   Passed    0.02 sec
      Start 14: verbs_test
14/14 Test #14: verbs_test .......................   Passed    0.00 sec

100% tests passed out of 14

Total Test time (real) =   0.57 sec
WINDOWS ORACLE PASS static (/nix/store/l3kbaypdnl4m2w7h2dwa4l5cifw78dnx-reaper-speed-reaper-x86_64-w64-mingw32-0.1.0/reaper/reaper_speed_reaper.dll)
[0/2] Re-checking globbed directories...
ninja: no work to do.
PASS=True REASONS=[]
ORACLE PASS host
bash: line 1: rg: command not found
```

Result: exit 0; verify-failed; 2 files changed, 38 insertions(+), 1 deletion(-); log: logs/40-cleanup-L51-a3-1792032144577.log

### Replan (depth 0) — REWRITE → "3. One REAPER API list (Verify without rg)"
Trigger: 3 attempt(s), last: exit 0, verify-failed
Why: Attempt 3 finished the entire task — reaper_api.h, the REAPERAPI_IMPLEMENT split, and tools/check_api_want are all in the tree and the worker's own verify passed — but ralph's re-run died on 'bash: line 1: rg: command not found': the Verify uses `rg`, which is not on PATH in the verify environment, so `test "" -eq 1` fails. Only the Verify is wrong. (Task 4's Verify also uses rg and will need the same grep fix.)
Rejected: BUDGET: not a timeout and no time was wasted — the work was complete and the failure is a missing binary in the verify env. SPLIT/SPIKE: nothing left to build or decide. BLOCK: the fix is mechanical, no human input needed.
Log: logs/40-cleanup-L51-replan-1792032755675.log

## Task: 3. One REAPER API list (Verify without rg)

### Attempt 1 — 2026-10-15T02:54:53Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Sanity-check: `grep -rl 'REAPERAPI_WANT_' plugin/src` → only `plugin/src/reaper_api.h` (1 file). No `keep in sync` comment anywhere in plugin/ or tools/. `tools/check` calls `tools/check_api_want` (line 13). Working tree clean except this record file; attempt 3's commit 44192a0 is in history.
  - DONE: verify40-3b (corrected grep-based Verify) rc=0 in 11s (cached builds): tools/check green (14/14 unit tests, WINDOWS ORACLE PASS static, ORACLE PASS host), grep file count = 1, ORACLE PASS reaper/load. Line 51 ticked.

Result: exit 0; done; 2 files changed, 9 insertions(+), 1 deletion(-); log: logs/40-cleanup-L51-a1-1792032893416.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/40-cleanup.md
 M vibes/40-cleanup.md
```

## Task: 4. Move the REAPER sinks out of `main.cpp`

### Attempt 1 — 2026-10-15T02:56:08Z — budget 1800s

Facts:
- No partial work in tree; started from clean HEAD (only the record file was dirty).
- Created `plugin/src/reaper_sinks.h` (class decls, moved verbatim from main.cpp) and `plugin/src/reaper_sinks.cpp` (method bodies verbatim; includes `reaper_api.h` without REAPERAPI_IMPLEMENT, like reaper_project.cpp; all REAPER APIs used are already in the WANT list, so reaper_api.h untouched).
- `g_reaper_sink` / `g_action_sink` globals STAY in main.cpp (anon namespace) — they are the wiring into JogEngine/Router/VerbRouter; only the class definitions moved. `main.cpp` now `#include "reaper_sinks.h"`.
- Added `src/reaper_sinks.cpp` to the plugin-only source list in `plugin/CMakeLists.txt` (NOT PLUGIN_PURE_SOURCES), next to reaper_project.cpp.
- Updated the stale comment in `action_sink.h` ("REAPER implements this in main.cpp" → reaper_sinks.cpp).
- `cmake` is not on PATH in the worker shell; `tools/check` auto-falls back to `nix develop --command`, so run tools/check (or bg-run it) rather than calling cmake directly.
Facts:
- First task4-verify run failed at `tools/check`'s Windows step: `nix build .#reaper-plugin-windows` couldn't see `src/reaper_sinks.cpp` (untracked file — nix path dep ignores untracked files, see Learnings). Fixed with `git add` of the two new files; re-run then passed: ORACLE PASS static, ORACLE PASS host, ORACLE PASS reaper/load, chain rc=0. `rg` IS on PATH in this env (exit 1 = no match in main.cpp).
- `tools/reaper-oracle --layer reaper` without `--scenario` runs only the `load` scenario (the full 17-scenario suite is `--layer all`). The stale `verbs` scenario only bites `--layer all`, so it doesn't affect this task's Verify.
Next:
- DONE — line 68 ticked. No commit made (ralph commits).

Result: exit 0; done; 7 files changed, 153 insertions(+), 93 deletions(-); log: logs/40-cleanup-L68-a1-1792032968466.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M plugin/CMakeLists.txt
 M plugin/src/action_sink.h
 M plugin/src/main.cpp
A  plugin/src/reaper_sinks.cpp
A  plugin/src/reaper_sinks.h
 M vibes/.ralph/40-cleanup.md
 M vibes/40-cleanup.md
```

## Task: 5. Pure `Session`: the timer loop becomes testable

### Attempt 2 — 2026-10-15T03:00:51Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Working tree was clean (no partial work from attempt 1).
- Created `plugin/src/session.h/.cpp`: `IDevice : IWheelSource` (PopButton/PopLog/Connected/AbsolutePosition/RequestWheelMode/RequestJogLeds/RequestLeds) — `HidWheelSource` already satisfies it via its existing methods. `Session(IDevice&, IActionSink&, IProject&, IPlayheadSink&)` owns GestureEngine + Router + VerbRouter + nested HoldGatedSource + ModeGatedSource + JogEngine; `Tick(now_ms)` = button split (KEY_JOG/SCRL/SHTL/STOP_PLAY → router, else gestures), gesture poll → verbs, TakeModeChange → device, connect edge re-send (mode+LEDs, led_mask=-1), time-based LED poll (≥250 ms since last poll, push on change only), fixed 8 ms engine steps ≤50 ms per Tick, backlog capped at 250 ms, `timer gap`/`timer call` traces kept.
- `main.cpp` onTimer is now 11 lines (≤15): compute now_ms, `g_session->Tick(now_ms)`, drain `g_hid.PopLog` → ShowConsoleMsg. Removed from main: HoldGatedSource, gesture/verb/router globals, LED-mask + connect-edge state, GestureKindName/WheelModeName/ApplyModeToHid.
- Added `src/session.cpp` to PLUGIN_PURE_SOURCES (tests get it for free; session_test auto-globbed by test/CMakeLists.txt).
- Stall math for the test (corrected): 992 ms stall → acc=250 → 7 steps (56 ms) that Tick → 8 scrubs; then 7/7/7/7/1/1 over the next six 8 ms Ticks → total at t=1048 = 38 (1 + 31 backlog + 6 steady), 39 at t=1056.
- Verb test key: KEY_SNAP Tap → cmd 1157 (non-double-tap key, Tap fires at release).
- `HidWheelSource` now derives from `IDevice` (was `IWheelSource`); `hid_wheel_source.h` includes `session.h` (no include cycle: session.h never includes hid headers).
- LED poll/push is now `connected && (...)` in `Session::Tick` — pushing to an absent device is pointless; reconnect still force-pushes via `led_mask_=-1`.
- Catch-up loop consumes 7 steps (56 ms) per Tick: the `consumed < 50` check happens before the +8 ms, and 56 is the first step-multiple above 50 — same as the pre-refactor main.cpp loop.
- Native build needs `nix develop` (no cmake on PATH); running in bgrun job `build50`.

Result: full Verify (job `verify50`) exited 0 — `tools/check session` (build warning-free, session_test passed, Windows static oracle, host oracle), onTimer = 11 lines ≤ 15, `reaper-oracle --layer reaper` (load scenario) PASS. Line 81 ticked; committed as `40-cleanup: 5. Pure Session: the timer loop becomes testable`.

Result: exit 0; done; log: logs/40-cleanup-L81-a2-1792033252016.log

### Done — 2026-10-15 — 2 attempt(s)
Files:
```
M vibes/.ralph/40-cleanup.md
```

## Task: 6. `WheelDispatcher` replaces the wheel decorators

### Attempt 3 — 2026-10-15T03:12:39Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Clean tree at start (only the record file dirty); no partial work from attempts 1–2 (no earlier code, just a stale `### Attempt 3` heading left by ralph).
- New `plugin/src/wheel_dispatcher.h` (header-only, no CMake source change): `IWheelDevice : IWheelSource` adds `AbsolutePosition()`; `WheelDispatcher(IWheelDevice&, GestureEngine&, Router&)` is the IWheelSource for JogEngine — Tick: hold → `gestures.OnWheel(delta, 0)` + delta 0; else Jog → passthrough, Scrl → `router.OnWheel(delta)` + 0, Shtl → `router.OnShuttleOffset(device.AbsolutePosition())` + 0.
- `session.h`: `IDevice : IWheelDevice` (AbsolutePosition moved to the base), `HoldGatedSource` inner class deleted, `hold_gated_`/`gated_` members replaced by `WheelDispatcher dispatcher_`; `session.cpp` catch-up loop no longer calls `router_.OnShuttleOffset` (dispatcher does it per engine step).
- `router.h`: `ModeGatedSource` + its doc paragraph + `#include "jog_engine.h"` deleted. `jog_engine.h` header comment updated.
- `router_test.cpp`: ModeGatedSource case + FakeWheelSource deleted, header comment points at wheel_dispatcher_test.cpp.
- New `plugin/test/wheel_dispatcher_test.cpp`: fake IWheelDevice + FakeSink + real GestureEngine/Router; one case per branch (Jog passthrough/no rate traffic, Scrl scroll + 0 to engine, Shtl delta-inert + absolute rate + dead-band HardStop) and one hold case spanning Jog→Scrl→Shtl (HoldRotate for 45/9/5, zero scroll/rate/stop traffic).
- `SummedWheelSource`/`NullWheelSource` were already gone (task 2). `rg 'ModeGatedSource|HoldGatedSource' plugin` clean after the edits (docs/plugin-design-m1-m3.md still mentions ModeGatedSource historically; Verify only greps plugin/).
- New files `git add`ed before the nix Windows build (untracked files are invisible to the nix source copy — see Learnings).
- Build job: `check6` = `tools/check 'wheel_dispatcher|router|session'`.
Dead ends:
- router.h dropped `#include "jog_engine.h"` → compile errors in router.cpp and router_test.cpp (`JogEngine::kSecondsPerWheelUnit`) — both now include jog_engine.h themselves. Fixed.
- The first Verify run failed on the grep step because my own wheel_dispatcher.h header comment named the deleted classes — reworded to "hold/mode decorator chain".
Next:
- DONE — exact Verify (job `verify6`) rc=0: tools/check 'wheel_dispatcher|router|session' green (3/3: router_test, session_test, wheel_dispatcher_test; WINDOWS ORACLE PASS static; ORACLE PASS host), grep clean, ORACLE PASS reaper/load. Line 93 ticked.

Result: exit 0; done; log: logs/40-cleanup-L93-a3-1792033959148.log

### Done — 2026-10-15 — 3 attempt(s)
Files:
```
M vibes/.ralph/40-cleanup.md
```

## Task: 7. Merge `hold_nudge` into `nudge`

### Attempt 4 — 2026-10-15T03:21:07Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Clean tree at start; no partial work from attempts 1–3.
- `HoldNudge` moved into `nudge.h` (class + doc, `#include <cstdint>` added) and `nudge.cpp` (implementation, verbatim). `hold_nudge.h/.cpp` deleted.
- `verbs.h` was the only includer of hold_nudge.h (member `HoldNudge nudge{4}`) → now includes `nudge.h`. verbs.cpp already included nudge.h for the Nudge* functions; its comment on line 286 names HoldNudge (still valid).
- `hold_nudge_test.cpp` cases merged verbatim into `nudge_test.cpp` main() before `check_failures()` (check.h CHECK macro style matches).
- `plugin/test/CMakeLists.txt` needed NO change: tests are globbed via `file(GLOB *_test.cpp)` — deleting hold_nudge_test.cpp is the whole update.
- `plugin/CMakeLists.txt` PLUGIN_PURE_SOURCES: hold_nudge.cpp line removed.
- No new files, so no `git add` needed before the nix Windows build.
- DONE: verify7 (exact Verify) rc=0 in 14s — tools/check nudge: 13/13 tests build warning-free, nudge_test PASS (with merged HoldNudge cases), WINDOWS ORACLE PASS static, ORACLE PASS host; `ls plugin/src/hold_nudge.*` finds nothing. Line 104 ticked.
