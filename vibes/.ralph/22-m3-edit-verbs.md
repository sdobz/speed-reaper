# Ralph record: 22-m3-edit-verbs

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- NEW source files must be `git add`ed before `tools/check` passes: the Windows
  cross-build (nix build .#reaper-plugin-windows) packs the tree via git, so
  untracked .cpp files are missing while CMakeLists (modified, tracked) already
  references them → silent CMake configure failure.
- `tools/check` output after ctest: `WINDOWS ORACLE PASS static` then
  `ORACLE PASS host`; no banner is printed for these (objdump is already on
  PATH in the nix shell).


## Task: 1. `GestureEngine` (pure)

### Attempt 1 — 2026-10-12T15:21:05Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: exit 1; no-progress; log: logs/22-m3-edit-verbs-L20-a1-1791818465367.log

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 1 attempt(s), last: exit 1, no-progress
Log: logs/22-m3-edit-verbs-L20-replan-1791818472326.log

### Attempt 1 — 2026-10-12T15:24:25Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: exit 1; no-progress; 2 files changed, 4 insertions(+), 2 deletions(-); log: logs/22-m3-edit-verbs-L20-a1-1791818665326.log

### Attempt 1 — 2026-10-12T15:30:26Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: prior attempts left no code (only vibes edits); clean slate. Design §7 in docs/plugin-design-m1-m3.md (line 105) has exact types/signatures. cmake is not on PATH — `tools/check` execs into `nix develop` itself; run it via bgrun (includes Windows cross-build). Wrote plugin/src/gesture.h/.cpp (per-key state: pressed/hold_started/waiting_second/suppress_release_tap; std::map for deterministic emit order; OnWheel routes to earliest-pressed key, no-op when idle), plugin/test/gesture_test.cpp (6 tests, check.h style), added gesture.cpp to PLUGIN_PURE_SOURCES. HoldActive() = true while any key is down (superset of "between start and end", needed so caller routes wheel to gesture pre-HoldStart).
- Facts: `tools/check gesture` running in background job `gesture-check` (log vibes/.ralph/logs/gesture-check.log).
- Dead ends: first `tools/check gesture` run (job `gesture-check`) failed with rc=1 and no visible error: `tools/windows-oracle` hides its `nix build .#reaper-plugin-windows` stderr (`2>/dev/null`) and the nix source pack (git-based) lacked untracked gesture.cpp → CMake "Cannot find source file". Also: `cmd | tail; echo rc=$?` reports tail's status, not the command's — earlier "pass" reading of windows-oracle was bogus (reaper-oracle host really had passed).
- Next: DONE — `tools/check gesture` green (job `gesture-check2`: gesture_test pass, WINDOWS ORACLE PASS static, ORACLE PASS host) after `git add`-ing the new files. Box ticked.

Result: exit 0; done; log: logs/22-m3-edit-verbs-L20-a1-1791819026491.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M vibes/.ralph/22-m3-edit-verbs.md
```

## Task: 2. `VerbRouter` and the command-id table (pure)

### Attempt 2 — 2026-10-12T15:38:05Z — budget 1800s

- Facts:
  - Design §7 table has 11 rows (IN/OUT each get Tap and DoubleTap rows → 40635); Hold* kinds are unmapped → no-op.
  - Wrote plugin/src/verbs.h/.cpp (table `kVerbTable` at top of verbs.cpp, `VerbDoubleTapKeys()` = {KEY_IN, KEY_OUT, KEY_ESC}), plugin/test/verbs_test.cpp (FakeSink), added verbs.cpp to PLUGIN_PURE_SOURCES in plugin/CMakeLists.txt. All new files `git add`ed.
  - cmake is not on the default PATH; `tools/check` re-execs under `nix develop` automatically.
  - `tools/check verbs` running in background job `verbscheck` (log: vibes/.ralph/logs/verbscheck.log).
  - First run (job `verbscheck`) failed rc=1 with no visible error after ctest: new files were `git add`ed concurrently with the check; the git-packing Windows cross-build raced the staging → silent CMake configure failure (same class as task 1).
  - Second run (job `verbscheck2`) passed rc=0: verbs_test, WINDOWS ORACLE PASS static, ORACLE PASS host.
- Dead ends:
  - Starting `tools/check` before `git add`ing new sources → silent rc=1 at the cross-build stage. Stage first, then check.
- Next:
  - DONE: line 33 ticked; commit pending.

Result: exit 0; done; log: logs/22-m3-edit-verbs-L33-a2-1791819485510.log

### Done — 2026-10-12 — 2 attempt(s)
Files:
```
M vibes/.ralph/22-m3-edit-verbs.md
```

## Task: 3. Wire gestures + verbs into the plugin

### Attempt 3 — 2026-10-12T15:41:18Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: tree was clean; tasks 1–2 already landed (gesture.h/.cpp, verbs.h/.cpp in PLUGIN_PURE_SOURCES). main.cpp already had ReaperActionSink tracing `cmd %d` in RunCommand, so verb commands get the cmd trace for free.
- Facts: wiring added to plugin/src/main.cpp: globals `g_verb_router(g_action_sink)` + `g_gestures`; `SetDoubleTapKeys(VerbDoubleTapKeys())` in entry; onTimer() splits PopButton() by key (KEY_JOG/SCRL/SHTL/STOP_PLAY → Router only, else GestureEngine::OnKey with steady_clock now_ms); `Poll(now_ms)` every tick + `Pop` loop tracing `gesture %04x <kind>` (kind names tap/double_tap/hold_start/hold_rotate/hold_end) into VerbRouter::OnGesture. Poll/Pop placed before the `if (!g_engine) return` so gestures work regardless of the demo engine. OnWheel intentionally NOT wired (task says hold+rotate deltas are not routed yet). No new files, no CMake changes.
- Facts: `tools/check` (full) PASSED rc=0 on first run (job `m3wire`): 8/8 ctest, WINDOWS ORACLE PASS static, ORACLE PASS host.
- Next: DONE — line 45 ticked; commit pending.

Result: exit 0; done; log: logs/22-m3-edit-verbs-L45-a3-1791819678655.log

### Done — 2026-10-12 — 3 attempt(s)
Files:
```
M vibes/.ralph/22-m3-edit-verbs.md
```
