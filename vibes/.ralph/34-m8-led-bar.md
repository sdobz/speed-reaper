# Ralph record: 34-m8-led-bar

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `GetToggleCommandState(id)` (0/1/-1) also works on REAPER's "Set ..." edit-mode commands (40309/40310/40311) — not only on "Toggle" actions (41990/41991, 1157, 41117). Fresh REAPER profile defaults: snap ON, trim-behind OFF, ripple off.
- `plugin_host` (host oracle layer) resolves every REAPER API name to one generic `void` stub — return values are register junk. Code that runs in the timer must treat stub returns as junk-tolerant (no `vector::reserve` on stub counts, no pointer chasing on stub item pointers); scalar `== 1` / `& 1` reads are fine.


## Task: 1. Research: prove the state reads

### Attempt 1 — 2026-10-14T06:21:35Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts: `GetToggleCommandState` returns 0=off, 1=on, -1=NA (not a toggle) — reaper-api.htm §GetToggleCommandStateEx. Main-section ripple actions: 40309/40310/40311 are *Set* commands; the real toggles are 41990 "Toggle ripple editing per-track", 41991 "Toggle ripple editing all tracks", 1162 "Toggle ripple editing on/off", 1155 cycles (docs/reaper-actions.tsv). No GetSet API for ripple mode exists (grep of reaper-api.htm) — GetToggleCommandState on a toggle action is the only query.
- Facts: wrote `oracle/scenarios/led_reads.lua` (probe version, settled start, no .env / fake device); records GetToggleCommandState of 1157, 40310, 40311, 41117, 41990, 41991, 1162 after baseline / ripple off / per-track / all / trim-behind toggle (41117, before-read) / snap toggle (1157).
- Facts: MEASURED (REAPER 7.81, fresh profile): design §5 candidate ids ALL work — `GetToggleCommandState(40310)` = 1 only in per-track, `40311` = 1 only in all (the "Set ripple editing" commands ARE valid toggle queries, no -1); `41990`/`41991` (Toggle ripple per-track/all) and `1162` (on/off) track identically (1162=1 for both modes). `41117` toggles 0→1, `1157` 1→0, both exact (after == 1-before). Fresh-profile defaults differ: snap ON, trim-behind OFF, ripple off — read, don't assume. No ripple Get/Set in the Lua API; GetToggleCommandState is the only read. Design §5 ids correct — no fix made.
- Facts: final scenario `oracle/scenarios/led_reads.lua` asserts tracking (ripple off/per/all → 40310/40311 combos; trim-behind and snap after==1-before) and records all 7 ids as check details; passes. `docs/notes/m8-state-reads.md` written with the measured table. Verify command exits 0. Ticked task 1; done.

Result: exit 0; done; log: logs/34-m8-led-bar-L17-a1-1791958895218.log

### Done — 2026-10-14 — 1 attempt(s)
Files:
```
M vibes/.ralph/34-m8-led-bar.md
```

## Task: 2. `LedState` + `MainLedMask` (pure)

### Attempt 2 — 2026-10-14T06:26:32Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: unit tests live in `plugin/test/` (not plugin/tests). `LED_*` bit indexes and `ledForCam(1..9)` (returns -1 out of range) are in `src/speed_editor/key_map.h` — CAM bits are non-contiguous (CAM1=14 … CAM9=8), so use `ledForCam` for `active_take`.
- Facts: design §5 mapping: SNAP=snap, CUT=ripple==1, DIS=ripple==2, LIVE_OWR=trim_behind, CAM_n=active_take, VIDEO_ONLY=isolate, AUDIO_ONLY=solo.
- Facts: rewrote `plugin/src/led_state.h/.cpp` (LedState struct with default-init members, `MainLedMask(const LedState&)`), `plugin/test/led_state_test.cpp` (snap/ripple/trim/take/isolate/solo/combo cases), and the main.cpp call site (builds a LedState with only snap set, no new reads).
- Facts: running `tools/check led_state` via bgrun job `check_led` (log vibes/.ralph/logs/check_led.log) while writing this. Result: rc=0 — led_state_test passed, Windows static + host oracle pass. Task 2 ticked.

Result: exit 0; done; log: logs/34-m8-led-bar-L29-a2-1791959192497.log

### Done — 2026-10-14 — 2 attempt(s)
Files:
```
M vibes/.ralph/34-m8-led-bar.md
```

## Task: 3. Read the state in the timer loop

### Attempt 3 — 2026-10-14T06:31:20Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: only two IProject implementations exist (ReaperProject, plugin/test/fake_project.h). ReaperProject already had private `isolate_active_`/`solo_active_` (M6) — `IsolateActive()`/`SoloActive()` are one-line accessors.
- Facts: added `LedState ReadLedState()` + `bool IsolateActive()`/`SoloActive()` to IProject (project.h now includes led_state.h, pure). ReaperProject::ReadLedState: snap=1157, ripple=RippleMode() (40310/40311), trim_behind=41117, active_take = 1-based I_CURTAKE shared by all selected items (0 if none/mixed/take>9/no active take; single EnumerateItems pass, no new REAPER APIs → no new REAPERAPI_WANT lines needed, only comment updates in main.cpp).
- Facts: main.cpp 250 ms block now `MainLedMask(g_project.ReadLedState())`, push only on change, reconnect edge re-send kept (g_snap_mask → g_led_mask). FakeProject gained `snap`/`trim_behind` members + ReadLedState (shared take from selection + active_takes/take_counts) + IsolateActive/SoloActive; new test plugin/test/led_read_test.cpp (auto-globbed by plugin/test/CMakeLists.txt).
- Dead ends: first full check (job `check3`) failed: new `led_read_test` bug (fake ignored `active_takes` when `take_counts` was empty — fixed), then host oracle ABORTed: `std::length_error` in `EnumerateItems::reserve`. Root cause: `plugin/tools/plugin_host.cpp` resolves every REAPER 7.81 API name to one generic `void` stub, so `CountMediaItems` returns register junk — negative → `reserve` throws. The host layer used to be safe because onTimer only read junk-tolerant scalars. Fix: `if (n < 0) n = 0;` guard in `EnumerateItems()` (real REAPER never counts < 0; host re-run rc=0).
- Facts: full `tools/check` runs: `check3` (both failures above), `check4` (test fixed; host oracle aborted — guard added), `check5` → rc=0 (14/14 tests, Windows static, host oracle). Line 41 ticked.
- Learnings: see top — `plugin_host` stub-API junk-return rule (caused the check4 abort).

Result: exit 0; done; 7 files changed, 109 insertions(+), 17 deletions(-); log: logs/34-m8-led-bar-L41-a3-1791959480382.log

### Done — 2026-10-14 — 3 attempt(s)
Files:
```
M plugin/src/main.cpp
 M plugin/src/project.h
 M plugin/src/reaper_project.cpp
 M plugin/src/reaper_project.h
 M plugin/test/fake_project.h
 M vibes/.ralph/34-m8-led-bar.md
 M vibes/34-m8-led-bar.md
?? plugin/test/led_read_test.cpp
```

## Task: 4a. Scenario `leds_ripple`: ripple and trim-behind bits

### Attempt 4 — 2026-10-14T06:40:58Z — budget 1800s

- Facts: LED bits (src/speed_editor/key_map.h): CUT=1 (0x02), DIS=2 (0x04), SNAP=5 (0x20), LIVE_OWR=9 (0x200). main.cpp:375 polls ReadLedState every 31 timer ticks (~30 Hz) and traces `led main=%x` only on change; g_led_mask=-1 forces a first read at load (fresh profile: snap ON, ripple off, tb off → 0x20). Plugin has NO verb buttons for ripple/trim-behind → Lua drives state via Main_OnCommand 40309/40310/40311/41117; device taps ESC (0x31) single-tap at 400 ms as the harmless needed tap (cmd 40635, clears time sel — no LED effect).
- Facts: trace line format "<ms since plugin load> <msg>" (trace.cpp); Lua clock time_precise is a different epoch — calibrate offset via the `log connected` trace line at body start.
- Facts: `log connected` trace line is `log connected (serial FAKE0001)` (not bare) — calibrate with prefix match. First `led main=20` traced at ~237 ms (snap ON, no ripple/tb). ESC single tap traces `gesture 0031 tap` + `cmd 40635`, no LED effect. v0 check + device tap + offset all pass.
- Dead ends: regex `log connected$` (suffix breaks it); `local b = l:match(2 captures)` drops the 2nd capture (use `local ms, hx = ...`); `within()` referenced undefined upvalue `v` → format(nil).
- Dead ends: comparing calibrated absolute trace times vs step times relative to body start — must store step times as `st + off`.
- Facts (final, PASS): measured masks in order 0x20 (off, snap ON) → 0x22 (CUT) → 0x24 (DIS) → 0x224 (DIS+LIVE_OWR=0x200); each `led main=` line lands 200–850 ms after its Main_OnCommand step (poll = every 31 timer ticks). All 9 checks pass; verify `tools/reaper-oracle --layer reaper --scenario leds_ripple` → ORACLE PASS.
- Done: added oracle/fake/leds_ripple.txt (ESC single tap 0x31 @400 ms), oracle/scenarios/leds_ripple.env, oracle/scenarios/leds_ripple.lua; added `leds_ripple` to REAPER_SCENARIOS in tools/reaper-oracle (suite list).

Result: exit 0; done; log: logs/34-m8-led-bar-L56-a4-1791960058153.log

### Done — 2026-10-14 — 4 attempt(s)
Files:
```
M vibes/.ralph/34-m8-led-bar.md
```

## Task: 4b. Scenario `leds_cam`: CAM take bit

### Attempt 5 — 2026-10-14T06:51:06Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: LED_CAM2 = bit 15 (src/speed_editor/key_map.h) → CAM2 mask = 0x8000. ReadLedState (reaper_project.cpp:175): active_take = 1-based I_CURTAKE shared by ALL selected items, else 0 (mixed → 0 → no CAM bit). No plugin source changed → no tools/check rebuild needed.
- Facts: device taps ESC (0x31, cmd 40635, no LED effect) for device-path coverage; CAM1..9 buttons deliberately NOT used — CamSetTake (verbs.cpp:122) would change active takes itself and race the Lua steps. Lua: `InsertTrackAtIndex(-1)` + `AddMediaItemToTrack` + 3×`AddEmptyTake` + `I_CURTAKE` (0-based: take 2 = 1) + `B_SELECTED` + `UpdateItemInProject`; empty takes are fine (only I_CURTAKE + selection read).
- Facts: steps: ~0 baseline (nothing selected → snap-only mask, CAM2 must be absent from every line up to 1 s); 1600 ms 3-take item take-2 selected → 0x8020; 3200 ms second 3-take item take-1 also selected (mixed) → back to 0x20 (mask *does* change vs 0x8020 so a new `led main=` line is traced even though the value equals the baseline). wait_ms(5200); pattern copied from leds_ripple.lua (calibrate on `log connected`, `led main=` within [step-300, step+1500]).
- Facts: added `leds_cam` to REAPER_SCENARIOS suite list in tools/reaper-oracle; files oracle/fake/leds_cam.txt, oracle/scenarios/leds_cam.env, oracle/scenarios/leds_cam.lua written. Baseline v0 check passed on run 1 (fresh profile: snap ON → 0x20, no CAM bit).
- Dead ends: `reaper.InsertTrackAtIndex(-1)` — the `isFolder` bool is a required 2nd arg ("bad argument #2 ... boolean expected").
- Dead ends: `InsertTrackAtIndex` returns **bool**, not a MediaTrack (REAPER 7.81 lua; same as hold_move_trim.lua:28) — the track must be fetched with `reaper.GetTrack(nil, 0)`; `AddMediaItemToTrack(nil)` → "MediaTrack expected". Also switched selection to `SetMediaItemSelected`/`IsMediaItemSelected` (pattern in hold_move_trim.lua) + `UpdateArrange()` after item creation.
- Dead ends: `reaper.AddEmptyTake` is NOT exposed in this REAPER 7.81 lua env — use `reaper.AddTakeToMediaItem(item, true)` (3× for a 3-take item; items start with 0 takes — pattern from simple_verbs.lua:45-46, no source needed, plugin only reads I_CURTAKE/selection). The first two failed runs were my own Lua API misuses (the scenario's checks never even ran); the two-attempt rule starts counting once a check actually fails.
- Facts (final, PASS run 4, job `ledscam4`, log vibes/.ralph/logs/ledscam4.log): all 9 checks green — baseline `led main=20` (snap ON, nothing selected, no CAM); step 1 (3-take item, take 2 selected) → `led main=8020` ~687 ms after the step; step 2 (second 3-take item take 1 also selected, mixed) → `led main=20` ~170 ms after; order + take/selection readbacks all pass. `leds_cam` added to the REAPER_SCENARIOS suite list (after leds_ripple). Task 4b ticked; done.
