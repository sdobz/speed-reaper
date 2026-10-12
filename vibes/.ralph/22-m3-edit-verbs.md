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

## Task: 4. SNAP LED follows REAPER's snap state

### Attempt 4 — 2026-10-12T15:44:06Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: prior attempt left no code (clean slate). LED_SNAP = bit 5 in src/speed_editor/key_map.h (byte0 bit5); key_map.h reached via `${REPO_ROOT}/src` include dir (already on path for plugin, hid_probe, Linux tests, and the WIN32 block). HidWheelSource::RequestLeds just marks pending_ dirty — safe to call pre-connect.
- Facts: wrote plugin/src/led_state.h/.cpp (`MainLedMask(bool)` = `snapOn ? 1u<<LED_SNAP : 0u`), plugin/test/led_state_test.cpp (3 checks), added led_state.cpp to PLUGIN_PURE_SOURCES (test CMakeLists globs *_test.cpp). main.cpp: REAPERAPI_WANT_GetToggleCommandState; global `int g_snap_mask = -1` (−1 = unsent); reconnect edge resets it; onTimer polls `GetToggleCommandState(1157)` every 31 ticks (~250 ms @30 Hz) or whenever g_snap_mask<0, traces `led main=%x` and calls RequestLeds only on change. No other LEDs, no ripple read.
- Next: DONE — `tools/check led_state` passed rc=0 on first run (job `ledcheck`): led_state_test pass, WINDOWS ORACLE PASS static, ORACLE PASS host. Line 60 ticked.

Result: exit 0; done; log: logs/22-m3-edit-verbs-L60-a4-1791819846445.log

### Done — 2026-10-12 — 4 attempt(s)
Files:
```
M vibes/.ralph/22-m3-edit-verbs.md
```

## Task: 5. Oracle scenario: `verbs` (also validates the command ids)

### Attempt 5 — 2026-10-12T15:46:51Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; 1 file changed, 5 insertions(+); log: logs/22-m3-edit-verbs-L74-a5-1791820011981.log

### Replan (depth 0) — SPLIT op=AND → "Fake device script: `oracle/fake/verbs.txt` + `verbs.env`", "Scenario `oracle/scenarios/verbs.lua` (validates the command ids)", "Add `verbs` to the default oracle suite"
Trigger: 5 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Why: Five attempts, zero scenario code: attempt 5's worker understood the whole oracle mechanism but stalled 180s in a design monologue (step windows, REAPER semantics) before writing anything. The task bundles 4+ artifacts and open design decisions, far beyond one 27B sitting; the log shows it needs the design decisions pre-settled and the work sliced per file.
Rejected: BUDGET: the record shows no real progress (no source files ever written) and the stall was monologue, not a long build, so the rules bar it and more time on the same-sized task would stall again. SPIKE: the only open questions (does time selection drive GetSet_LoopTimeRange, what SPLIT selects, snap/undo interplay) are exactly what one real oracle run settles — the spike would be verbs.lua itself, and the 'finish the parent' follow-up would still be the oversized task. REWRITE: the task text is clear and its Verify is correct; the problem is size, not clarity. BLOCK: nothing needs a human decision.
Log: logs/22-m3-edit-verbs-L74-replan-1791820482588.log

## Task: Fake device script: `oracle/fake/verbs.txt` + `verbs.env`

### Attempt 1 — 2026-10-12T16:01:13Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: wrote `oracle/fake/verbs.txt` (8 steps at 400, 800, 1200, 1600, 2000, 2400, 2800, 3200 ms; taps = down+up 50 ms; double-taps have press 2 down 100 ms after press 1 up, well inside the 300 ms window; order SPLIT 0x2F, IN 0x07, OUT 0x08, TRIM IN 0x09, IN double-tap, RIPL DEL 0x2B, SNAP 0x2E, ESC 0x31 double-tap, documented in header comment) and `oracle/scenarios/verbs.env` (`export SPEED_REAPER_FAKE_DEVICE=oracle/fake/verbs.txt`, same shape as buttons.env).
- Facts: `key 0x07 down` count = 3 (IN tap + double-tap pair), `key 0x31 down` count = 2; task Verify command passes.
- Next: DONE — line 76 of vibes/22-m3-edit-verbs.md ticked. Note for the verbs.lua task: fake-device event times are 400–3400 ms (script clock; fake device clock leads script clock ~40 ms per buttons.lua header); last event at 3400 ms, so the scenario needs >3.4 s of sampling plus double-tap promotion slack.

Result: exit 0; done; 4 files changed, 38 insertions(+), 1 deletion(-); log: logs/22-m3-edit-verbs-L76-a1-1791820873219.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
A  oracle/fake/verbs.txt
A  oracle/scenarios/verbs.env
 M vibes/.ralph/22-m3-edit-verbs.md
 M vibes/22-m3-edit-verbs.md
```

## Task: Scenario `oracle/scenarios/verbs.lua` (validates the command ids)

### Attempt 1 — 2026-10-12T16:02:38Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts: `Undo_CanUndo2` exists in the oracle REAPER (docs/reaper-api-subset.md:598, C: `const char* Undo_CanUndo2(ReaProject*)`) → Lua `reaper.Undo_CanUndo2()` returns a string.
- Facts: trace line formats confirmed from plugin/src/main.cpp: `gesture %04x %s`, `cmd %d`, `led main=%x` (plain, no jog=), `led jog=%x main=%x` (mode lines — the `led main=` match must not catch jog lines; a leading-space ` led main=` anchor does that).
- Facts: verb keys (0x2F/0x07/0x08/0x09/0x2B/0x2E/0x31) are all non-router keys (router: 0x1C/0x1D/0x1E/STOP-PLAY), so all go to the gesture engine.
- Next: run `tools/reaper-oracle --layer reaper --scenario verbs` DONE — line 83 ticked; `tools/reaper-oracle --layer reaper --scenario verbs` green (ORACLE PASS reaper/verbs, 16/16 checks). Facts: oracle REAPER (7.81) Lua bindings need explicit args: `InsertTrackAtIndex(0, true)` returns a bool (track = `GetTrack(nil, 0)`); `CountMediaItems(nil, track)`; `GetTrackMediaItem(track, i)` (no project arg); no `SetMediaItemInfo` (use `SetMediaItemInfo_Value`/`SetMediaItemSelected`); `GetSet_LoopTimeRange(false, false, 0.0, 0.0, false)` returns (start, end); `Undo_CanUndo2(nil)` returns the undo label string. REAPER 7.81 SPLIT (40012) DESELECTS both halves and 40511 (trim left) only trims *selected* items, so verbs.lua re-selects the items in the 1200-1580 ms script window before the TRIM step; the zero-length [0,5] half is dropped after trim (count 2→1, min pos 5). All 8 command ids in `plugin/src/verbs.cpp` validated correct — no id fixes, so no `docs/notes/m3-observations.md` entry needed. Verified effects: SPLIT count 1→2; IN start=5 / OUT end=5; IN x2 clears to (0,0); RIPL DEL count drops (1→0); SNAP flips GetToggleCommandState(1157) 1→0 and `led main=` ends at 0; ESC x2 changes Undo_CanUndo2 (“Delete items” → “Remove time selection”) with no cmd 40635 after the undo; trace gesture + cmd lines in exact script order. Next checkbox: add `verbs` to the default oracle suite in `tools/reaper-oracle`.

Result: exit 0; done; 3 files changed, 274 insertions(+), 1 deletion(-); log: logs/22-m3-edit-verbs-L83-a1-1791820958615.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
A  oracle/scenarios/verbs.lua
 M vibes/.ralph/22-m3-edit-verbs.md
 M vibes/22-m3-edit-verbs.md
```
