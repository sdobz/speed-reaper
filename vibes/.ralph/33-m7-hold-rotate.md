# Ralph record: 33-m7-hold-rotate

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/check` cross-builds the Windows plugin via nix, and the flake’s `src = ./.` copy includes only git-tracked files. `git add` any newly created source before relying on `tools/check` (or it fails with “Cannot find source file”).
- Controller key map (src/speed_editor/key_map.h): ESC = 0x31 (double-tap → cmd 40029 Edit: Undo); 0x1B is TIMELINE. Double-tap pattern in fake device files: down/up 50 ms apart, second press ≤300 ms after release (verbs.txt).
- Oracle scenarios: items made with `AddMediaItemToTrack` have **no take** — take-level props (D_STARTOFFS etc.) need `AddTakeToMediaItem(it,true)` + `SetMediaItemTake_Source` (smth_cut layout); `reaper.CountMediaItemTakes` is not in this build’s Lua API.


## Task: 1. `HoldNudge` accumulator (pure)

### Attempt 1 — 2026-10-14T05:25:10Z — budget 1800s

- Facts:
  - No partial work from earlier attempts; tree clean.
  - Wrote `plugin/src/hold_nudge.h/.cpp` (int64 accumulator, truncating division → remainders symmetric toward zero), `plugin/test/hold_nudge_test.cpp`, added `hold_nudge.cpp` to `PLUGIN_PURE_SOURCES` in `plugin/CMakeLists.txt` (alphabetical spot after gesture.cpp).
  - `cmake` is not on PATH; `tools/check` falls back to `nix develop --command`. Prebuilt `build/plugin/` exists, so iteration is incremental.
  - Test glob: `plugin/test/*_test.cpp` auto-becomes ctest `<name>`; test harness = `check.h` CHECK + `return check_failures();`.
- Dead ends:
  - First test draft had two arithmetic errors of its own (expected +1 step after net 2 ticks); the implementation was fine. Fixed expectations.
  - `nix build .#reaper-plugin-windows` (inside `tools/check`) failed with “Cannot find source file: hold_nudge.cpp” although the flake has `src = ./.`. Root cause: the flake’s source store copy includes **git-tracked** files (modified worktree contents included) but **skips untracked files**, so new .cpp/.h must be `git add`-ed before the Windows build can see them. After `git add`, the cross-build passes.
- Facts:
  - Staged (git add) the four new/changed files; NOT yet committed — ralph commits.
- Next:
  - DONE: `tools/check hold_nudge` exit 0 (job `check-hold-nudge2`); line 18 ticked; committed.

Result: exit 0; done; log: logs/33-m7-hold-rotate-L18-a1-1791955510904.log

### Done — 2026-10-14 — 1 attempt(s)
Files:
```
M vibes/.ralph/33-m7-hold-rotate.md
```

## Task: 2. `IProject` item geometry

### Attempt 2 — 2026-10-14T05:32:24Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; 1 file changed, 5 insertions(+); log: logs/33-m7-hold-rotate-L30-a2-1791955944555.log

### Replan (depth 0) — REWRITE → "2. `IProject` item geometry"
Trigger: 2 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Why: The worker stalled in a research monologue: it hunted the old REAPER name GetMediaTakeInfo_Value, found it nowhere, and spent 180s deciding whether take properties are even reachable — while the task's real blocker was never naming the 7.x functions (GetMediaItemTakeInfo_Value/SetMediaItemTakeInfo_Value + GetActiveTake) that the vendored SDK header does provide.
Rejected: SPLIT: any split (fake/interface first, ReaperProject second) breaks the build — a new pure virtual without the ReaperProject override fails tools/check, so no first child can pass its verify. SPIKE: the open question is factual and already answered (functions verified present in vendor/reaper-sdk/sdk/reaper_plugin_functions.h); the facts go straight into the body. BUDGET: record shows zero code progress and the run died at ~3 of 30 min monologuing, not from budget exhaustion. BLOCK: no human decision is needed.
Log: logs/33-m7-hold-rotate-L30-replan-1791956186691.log

### Attempt 1 — 2026-10-14T05:41:11Z — budget 1800s

Facts:
- Confirmed in vendor/reaper-sdk/sdk/reaper_plugin_functions.h: `TimeMap_curFrameRate(ReaProject*, bool* dropFrameOut)` (line ~7142), `GetActiveTake(MediaItem*)` (~1478), `GetMediaItemTakeInfo_Value` (~2087), `SetMediaItemTakeInfo_Value` (~6099). All four wired with REAPERAPI_WANT in both reaper_project.cpp and main.cpp.
- ReaperProject uses nullptr as the project (current project), so Fps() calls `TimeMap_curFrameRate(nullptr, &drop)`.
- Implemented: ItemGeom + Fps/GetGeom/SetGeom in project.h (pure), reaper_project.h/.cpp, fake_project.h (geoms vector + `double fps = 30.0`). No new files created, so no git add needed.
- Fake bad-id GetGeom returns {0,0,0,1,1}; valid-id missing-entry also {0,0,0,1,1} with pos/len from items[id]; Reaper bad-id returns {0,0,0,1,0} (vol 0 mirrors ItemVolume).

Result: DONE — `tools/check` exit 0 (job `geom-check`): 12/12 unit tests, Windows oracle DLL build + host oracle pass. Line 30 ticked [x].

Result: exit 0; done; 7 files changed, 102 insertions(+), 1 deletion(-); log: logs/33-m7-hold-rotate-L30-a1-1791956471233.log

### Done — 2026-10-14 — 1 attempt(s)
Files:
```
M plugin/src/main.cpp
 M plugin/src/project.h
 M plugin/src/reaper_project.cpp
 M plugin/src/reaper_project.h
 M plugin/test/fake_project.h
 M vibes/.ralph/33-m7-hold-rotate.md
 M vibes/33-m7-hold-rotate.md
```

## Task: 3. Pure nudge math

### Attempt 1 — 2026-10-14T05:44:14Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: design §4 table is in `docs/plugin-design-m5-m9.md` §4 (SPLIT=move pos; TRIM IN pos/len/startoffs*s*playrate; TRIM OUT len; SLIP SRC startoffs+=s*playrate; SLIP DEST pos+=s, startoffs-=s*playrate; AUDIO LEVEL 0.5 dB/step clamped 0…+12 dB). `ItemGeom.vol` is linear (1.0 = 0 dB) — NudgeVolume converts via 20*log10/pow.
- Facts: created `plugin/src/nudge.h/.cpp` (free functions, delta constrained to a [lo,hi] interval; conflicting clamps → no-op) + `plugin/test/nudge_test.cpp`; added `nudge.cpp` to `PLUGIN_PURE_SOURCES` (alphabetical, after led_state); git-added all four files. Native build with -Werror clean, `build/plugin/test/nudge_test` passes.
- Facts: `tools/check nudge` passed rc=0 (job `check-nudge`): ctest nudge_test + hold_nudge_test pass, Windows cross-build PASS, oracle host PASS. Line 49 ticked; task done.

Result: exit 0; done; log: logs/33-m7-hold-rotate-L49-a1-1791956654907.log

### Done — 2026-10-14 — 1 attempt(s)
Files:
```
M vibes/.ralph/33-m7-hold-rotate.md
```

## Task: 4. Hold rows in `VerbRouter`

### Attempt 2 — 2026-10-14T05:49:07Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - `VerbRouter` lives in `plugin/src/verbs.{h,cpp}` (no verb_router file).
  - Implemented: `kHoldRows` table (SPLIT/MOVE, TRIM IN/OUT, SLIP SRC/DEST, AUDIO LEVEL → nudge.h free fns + labels "Speed Reaper: …"); `VerbRouter::OnGesture` switch on HoldStart (TargetItems once, selection snapshot, Undo_Begin), HoldRotate (per-key `HoldNudge{4}` in `holds_` map; step = 1/Fps(); SetGeom on every target), HoldEnd (restore snapshot, Undo_End, erase). Unmapped keys / no-target holds do nothing. TIMELINE HoldRotate untouched (falls through to its composite row when no M7 hold is active).
  - main.cpp did NOT feed wheel deltas to `g_gestures` (OnWheel never called). Fix: new `HoldGatedSource` IWheelSource wrapping `g_hid`, inserted before `ModeGatedSource`: while `g_gestures.HoldActive()`, the delta goes to `g_gestures.OnWheel` and is zeroed for the jog/scroll path. `g_gestures` global moved above the wrapper (name lookup in inline member).
  - Added `test_hold_rotate()` to verbs_test.cpp: SPLIT +8 ticks → pos +2/30 s, one undo pair; no-target hold → nothing; selection snapshot restored at HoldEnd; unmapped ROLL hold → nothing; negative ticks.
  - No new untracked source files this task (all edits to tracked files), so no `git add` needed for the flake copy.
  - Plain PATH has no ninja/cmake; `tools/check` self-enters `nix develop`. Running full `tools/check verbs` via bgrun job `verbs-check-2` (log `vibes/.ralph/logs/verbs-check-2.log`).
- Next:
  - DONE: `tools/check verbs` green (rc=0; verbs_test pass, WINDOWS ORACLE PASS static, ORACLE PASS host). Line 62 ticked.

Result: exit 0; done; log: logs/33-m7-hold-rotate-L62-a2-1791956947411.log

### Done — 2026-10-14 — 2 attempt(s)
Files:
```
M vibes/.ralph/33-m7-hold-rotate.md
```

## Task: 5a. Scenario `hold_move_trim`: SPLIT and TRIM OUT holds

### Attempt 3 — 2026-10-14T05:57:20Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Fake grammar: `at <ms> wheel <delta>` / `at <ms> key <hex> down|up`; the wheel delta is the raw tick count passed straight through HoldGatedSource → `GestureEngine::OnWheel` → `HoldNudge::Add` (no scaling), so `wheel 2` = 2 ticks. One `hold_rotate` gesture per non-zero wheel event.
  - Keys (src/speed_editor/key_map.h): SPLIT 0x2F, TRIM OUT 0x0A. Hold recognition: `hold_ms=350` (gesture.h); key-up after a hold emits HoldEnd only (no tap). TRIM OUT is NOT a double-tap key, so no tap-defer interference.
  - TargetItems with empty selection = cursor item (target.cpp rule 2): cursor 2.5 inside A [0,5] → A is the hold target; selection is never touched by the hold path.
  - Wrote oracle/fake/hold_move_trim.txt (SPLIT down 300, 4×`wheel 2` @700–724, up 1200; TRIM OUT down 1600, 4×`wheel -2` @2000–2024, up 2500), oracle/scenarios/hold_move_trim.env + .lua (samples every defer cycle: count==1, B_UISEL==0; final pos≈2/30, len≈5−2/30 with 1 ms tol — covers fps 30 or 29.97; trace: collapsed hold_start/rotate/end per key in order + zero `cmd` lines), added `hold_move_trim` to REAPER_SCENARIOS in tools/reaper-oracle.
  - Plugin source unchanged since task 4 (which passed tools/check), so `--layer reaper` needs no rebuild.
  - Verify running as bgrun job `hmt-verify` (log vibes/.ralph/logs/hmt-verify.log).
- Next:
  - DONE first try: job `hmt-verify` → `ORACLE PASS reaper/hold_move_trim`, RC=0. Line 77 ticked [x].

Result: exit 0; done; log: logs/33-m7-hold-rotate-L77-a3-1791957440565.log

### Done — 2026-10-14 — 3 attempt(s)
Files:
```
M vibes/.ralph/33-m7-hold-rotate.md
```

## Task: 5b. Scenario `hold_slip_vol`: SLIP SRC and AUDIO LEVEL holds

### Attempt 4 — 2026-10-14T06:05:03Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Keys (src/speed_editor/key_map.h): SLIP SRC = 0x0C, AUDIO LEVEL = 0x2C. NudgeSlipSrc: startoffs += step*playrate (1/30 @ 4 ticks, playrate 1.0), pos/len fixed; NudgeVolume: +0.5 dB/step → vol 1.0 → 10^(0.5/20). ReaperProject::SetGeom writes item D_VOL and active-take D_STARTOFFS/D_PLAYRATE.
  - Wrote oracle/fake/hold_slip_vol.txt (0x0C down 300, 4×`wheel 1` @700–724, up 1200; 0x2C down 1600, 4×`wheel 1` @2000–2024, up 2500), oracle/scenarios/hold_slip_vol.env + .lua (same setup as 5a; final checks: startoffs≈1/30 ±1ms, pos/len unchanged, vol≈10^(0.5/20) ±1e-4, selection empty every sample, trace 000c/002c start/rotate/end, no cmd lines). Added `hold_slip_vol` to REAPER_SCENARIOS in tools/reaper-oracle.
  - Plugin unchanged since 5a (which needed no rebuild), so `--layer reaper` runs without tools/check.
  - Verify running as bgrun job `hsv-verify` (log vibes/.ralph/logs/hsv-verify.log).
- Dead ends:
  - First run: all checks passed except startoffs=-1 — `AddMediaItemToTrack` items have **no take** in this REAPER build (probe: `GetMediaItemTake(it,0)` nil, `GetActiveTake` nil), so `SetGeom`'s take branch (D_STARTOFFS) is skipped and the Lua read got nil. vol check passed (item-level D_VOL).
  - Fix: build the item like `smth_cut.lua` — `AddTakeToMediaItem(it,true)` + `SetMediaItemTake_Source` on a generated tone wav (6 s > item len 5 s). Re-running as job `hsv-verify2`.
- Facts:
  - `reaper.CountMediaItemTakes` is NOT in this build's Lua API (nil field). Use `GetMediaItemTake(it,0)`.
  - `REAPER_SCENARIOS` env var is ignored by tools/reaper-oracle (the script reassigns it); ad-hoc runs need `--scenario <name>` + matching `oracle/scenarios/<name>.lua/.env`. Probe scenario `probe_takes` deleted.
- Next:
  - DONE: job `hsv-verify2` → `ORACLE PASS reaper/hold_slip_vol`, rc=0 (startoffs=1/30, vol=10^(0.5/20), selection empty, trace 000c/002c clean). Line 90 ticked [x].

Result: exit 0; done; 3 files changed, 21 insertions(+), 2 deletions(-); log: logs/33-m7-hold-rotate-L90-a4-1791957903885.log

### Done — 2026-10-14 — 4 attempt(s)
Files:
```
M tools/reaper-oracle
 M vibes/.ralph/33-m7-hold-rotate.md
 M vibes/33-m7-hold-rotate.md
?? oracle/fake/hold_slip_vol.txt
?? oracle/scenarios/hold_slip_vol.env
?? oracle/scenarios/hold_slip_vol.lua
```

## Task: 5c. Scenario `hold_undo`: ESC restores a hold in one undo step

### Attempt 5 — 2026-10-14T06:11:29Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Plugin ESC key on the controller is **0x31** (not 0x1B; 0x1B is TIMELINE). `verbs.cpp`: `{KEY_ESC, Tap, 40635}`, `{KEY_ESC, DoubleTap, 40029}` (Edit: Undo). Double-tap window 300 ms (gesture.h `double_tap_ms`); verbs.txt pattern: down 1600/up 1650/down 1750/up 1800 → fires on press 2 (~1750), tap suppressed.
  - SPLIT hold undo label is "Speed Reaper: move" (kHoldRows, verbs.cpp line 294). One hold = one Undo_Begin/Undo_End block (task 4).
  - "One undo step, not eight" is verified **behaviorally**: `U_UNDOCNT` is not confirmed to exist in this Lua build, so the check is that a single cmd 40029 restores A.pos fully to 0 (eight separate blocks would leave pos 1/30) and `Undo_CanUndo2` label returns to the "setup" baseline. Mid-window check (script 1250-1650 ms): pos≈2/30 and label≠"setup".
  - No `mid`/`at` helpers in lib.lua — defined locally (as verbs.lua does); `window()` returns first/last index or nil.
  - Wrote oracle/fake/hold_undo.txt, oracle/scenarios/hold_undo.{env,lua}; added `hold_undo` to REAPER_SCENARIOS (tools/reaper-oracle line 239). Plugin unchanged since 5b → no rebuild needed.
  - DONE first try: bgrun job `hold_undo-verify` (log vibes/.ralph/logs/hold_undo-verify.log) -> ORACLE PASS reaper/hold_undo, rc=0. Line 103 ticked [x].

Result: exit 0; done; 3 files changed, 15 insertions(+), 2 deletions(-); log: logs/33-m7-hold-rotate-L103-a5-1791958289992.log

### Done — 2026-10-14 — 5 attempt(s)
Files:
```
M tools/reaper-oracle
 M vibes/.ralph/33-m7-hold-rotate.md
 M vibes/33-m7-hold-rotate.md
?? oracle/fake/hold_undo.txt
?? oracle/scenarios/hold_undo.env
?? oracle/scenarios/hold_undo.lua
```
