# Ralph record: 31-m5-targeting

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- **Commit new files before `tools/check`.** The Windows cross build (`nix build .#reaper-plugin-windows` → `tools/windows-oracle --layer static`) builds from the flake source `src = ./.`, which only carries **git-tracked** files. Untracked sources referenced by CMakeLists make it fail with "Cannot find source file … No SOURCES given to target: speed_reaper" and the script dies silently (`2>/dev/null`).
- `cmake`/`ctest` are not on PATH outside the dev env: `nix develop --command …` (or run `tools/check`, which self-execs into it).


## Task: 1. `IProject` + `TargetItems` (pure)

### Attempt 1 — 2026-10-13T01:01:26Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `cmake` is not on PATH outside the dev env; use `nix develop --command ...` (or just run `tools/check`, which self-execs into `nix develop`).
- Created `plugin/src/project.h` (IProject exactly per design §2), `plugin/src/target.h/.cpp` (TargetItems + WithTargetSelected template in header), `plugin/test/fake_project.h` (records SetSelectedItems calls in `selected_calls`, does NOT apply them), `plugin/test/target_test.cpp`; added `target.cpp` to `PLUGIN_PURE_SOURCES`.
- Rule 2 edge cases: boundary belongs to the item *starting* there (pos<=c<pos+len, items in pos order); cursor == pos+len of the track's LAST item → that item (1e-9 eps); else empty.
- IProject methods are non-const (per design §2) — helpers taking the project must take non-const refs.
- `target_test` alone passes (ctest -R target_test, 1/1).

Next:
- DONE. `tools/check target` exits 0 (ctest target_test 1/1, WINDOWS ORACLE PASS static, ORACLE PASS host). Line 16 ticked; work committed as `31-m5-targeting: 1. IProject + TargetItems (pure)`.

Dead ends:
- First background `tools/check target` (job `check-target`) failed rc=1 with no visible error: `tools/windows-oracle --layer static` dies silently because the flake cross build couldn't see untracked `target.cpp` (see Learnings). Fixed by committing; not a code bug.

Result: exit 0; done; 1 file changed, 1 insertion(+), 1 deletion(-); log: logs/31-m5-targeting-L16-a1-1791853286436.log

### Done — 2026-10-13 — 1 attempt(s)
Files:
```
M vibes/31-m5-targeting.md
?? vibes/.ralph/31-m5-targeting.md
```

## Task: 2. `ReaperProject` (REAPER-backed `IProject`)

### Attempt 2 — 2026-10-13T01:07:56Z — budget 1800s
Facts:
- Attempt 1 left no code; starting fresh.
- `plugin/src/project.h` (IProject) already exists from task 1; test fake at `plugin/test/fake_project.h`.
- `GetTrackAt`/`GetTrackIndex`/`GetSetLastTouchedTrack`/`GetSet_Loc` are NOT in the vendored `reaper_plugin_functions.h` (no `reaper.h` vendored). Subset doc §8 instead has `GetTrack(proj, idx)`, `CountTracks`, `GetLastTouchedTrack` — all also in `oracle/reaper_api_names.txt` (874 names, the host oracle's export list).
- `GetSet_Loc` is in NEITHER the subset doc NOR `reaper_api_names.txt` → resolve it via a manual `GetFunc("GetSet_Loc")` in a `ReaperProject::Init(void(*GetFunc)(const char*))` called from the entry after `REAPERAPI_LoadAPI`; oracle returns nullptr → `HasTimeSelection()` safely false. LOC_TSELSTART=0x20, LOC_TSELEND=0x21 (SDK reaper.h constants; could not verify online — web_search returned nothing, docs.reaper.com unreachable; step 5's oracle will exercise it).
- REAPERAPI pattern: main.cpp has `REAPERAPI_IMPLEMENT`+`REAPERAPI_MINIMAL`; reaper_project.cpp will define its own WANT set (mirrored in main.cpp) so a missing mirror is a link error on `REAPERAPI_<name>`.
- `tools/check` also runs `tools/reaper-oracle --layer host` (plugin_host + api names list) — all 12 new WANT names verified present in the list before adding them.
Facts (build fixes):
- **Vendored SDK uses `REAPERAPI_FUNCNAME(x)=x` (identity)** — the function pointers are plain `GetPlayState`, `CountMediaItems`, …, NOT `REAPERAPI_GetPlayState` (that prefix exists only in the upstream SDK where reaper_plugin.h redefines REAPERAPI_FUNCNAME). main.cpp calls them plain; reaper_project.cpp must too. This is the #1 gotcha for any future REAPER-API TU.
- `GetLastTouchedTrack()` takes NO parameters in this SDK (subset doc didn't show it; verified in reaper_plugin_functions.h:1914).
- reaper_project.cpp must include <algorithm>/<set> BEFORE reaper_project.h (SDK/Swell min/max macros break STL — same pattern as main.cpp's header comment).
- Files committed: `c555d0a` (ReaperProject) + `79acce2` (symbol-name/arity fixes). Native build + link green.
Next: `tools/check` running as bg job `check-m5t2` (log vibes/.ralph/logs/check-m5t2.log); if green, tick line 28 of vibes/31-m5-targeting.md.
Result: exit 0 (job check-m5t2): 11/11 unit tests, WINDOWS ORACLE PASS static, ORACLE PASS host. Line 28 ticked.

Result: exit 0; done; 2 files changed, 8 insertions(+), 2 deletions(-); log: logs/31-m5-targeting-L28-a2-1791853676491.log

### Done — 2026-10-13 — 2 attempt(s)
Files:
```
M vibes/.ralph/31-m5-targeting.md
 M vibes/31-m5-targeting.md
```

## Task: 3. `VerbRouter` takes `IProject`; composite rows

### Attempt 3 — 2026-10-13T01:21:53Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- `VerbRouter` is now `VerbRouter(IActionSink&, IProject&)`; `verbs.h` includes `project.h`. `verbs.cpp` has `kCompositeTable` (rows `{key, kind, void(*)(IActionSink&, IProject&)}`) next to `kVerbTable`; `OnGesture` checks the composite table first (composite rows win).
- TRIM IN/OUT removed from the id table; `TrimIn`/`TrimOut` composites do `Undo_Begin(); WithTargetSelected(p, [run 40511/40512]); Undo_End("Speed Reaper: trim")`. WithTargetSelected is header-only (template), so no new link deps.
- `main.cpp`: global `ReaperProject g_project;` defined just before `g_verb_router(g_action_sink, g_project)` (order-safe: ReaperProject has no ctor, methods only touch REAPER API at call time, post-Init).
- `verbs_test.cpp` now uses `fake_project.h`; existing tests unchanged in meaning (just an added `FakeProject` arg). New `test_trim_composites` covers: no selection → selects target `[0]` then restores `[]` (selected_calls `[[0],[]]`), undo block labelled `Speed Reaper: trim`; existing selection → single restore call, no extra selection.
- `tools/check verbs` green: verbs_test 1/1, WINDOWS ORACLE PASS static, ORACLE PASS host. (Remember: git-add before check — windows build only sees tracked files.)
Next: DONE — line 41 ticked, committed.

Result: exit 0; done; log: logs/31-m5-targeting-L41-a3-1791854513482.log

### Done — 2026-10-13 — 3 attempt(s)
Files:
```
M vibes/.ralph/31-m5-targeting.md
```

## Task: 4. RIPL DEL composite

### Attempt 4 — 2026-10-13T01:25:02Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- `tools/check verbs` green on prior attempts; prior `verbscheck*` logs were from earlier tasks, not partial task-4 work (tree was clean).
- Implemented: removed `{KEY_RIPL_DEL, Tap, 40006}` id row; added `RiplDel` composite (time sel → 40201; empty `TargetItems` → `Trace("target none")` + nothing; else one undo block, ripple off → 40310/40006/40309, ripple on → 40006, label "Speed Reaper: delete"). Undo label not specified by task/design — chose "Speed Reaper: delete" to mirror "Speed Reaper: trim".
- `verbs.cpp` now includes `trace.h` (REAPER-free, already in PLUGIN_PURE_SOURCES so it links into verbs_test).
- `verbs_test.cpp`: new `test_ripl_del_composite` — 5 scenarios (time sel → [40201] no churn; ripple off → [40310,40006,40309] + restore; ripple=1 → [40006]; no items → no commands/no churn; pre-selection → single restore call).
- Committed `98c55ea` before running check (windows-oracle sees only tracked files).
Next: DONE — `tools/check verbs` green (job `check-m5t4`: verbs_test 1/1, WINDOWS ORACLE PASS static, ORACLE PASS host). Line 54 ticked; code committed as `98c55ea`.

Result: exit 0; done; 3 files changed, 13 insertions(+), 4 deletions(-); log: logs/31-m5-targeting-L54-a4-1791854702946.log

### Done — 2026-10-13 — 4 attempt(s)
Files:
```
D docs/ReaperUserGuide781c/ReaperUserGuide781c.pdf:Zone.Identifier
 M vibes/.ralph/31-m5-targeting.md
 M vibes/31-m5-targeting.md
```

## Task: 5. TIMELINE select verbs

### Attempt 5 — 2026-10-13T01:28:10Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Tree was clean (no partial task-5 work). `tools/check verbs` = build + `ctest -R verbs` + windows static oracle + host oracle.
- main.cpp already routes every non-router key (JOG/SCRL/SHTL/STOP_PLAY) to the gesture engine via `default:` — TIMELINE needed no change there, verified.
- Implementation: `target.h/.cpp` now also export `TargetTrack(IProject&)` and `CursorTargetItem(IProject&)` (rule 2 only; `TargetItems` = rule 1 → `CursorTargetItem`). verbs.cpp composite rows now take `(VerbRouter&, IActionSink&, IProject&, const Gesture&)` so TIMELINE HoldRotate can accumulate `delta` in a `select_delta_` member (±kSelectStepTicks=20 per step, remainder kept). StepSelection: no selection on target track → + selects first / - last; else ±1 index clamped; exclusive selection; cursor untouched. Tap: rule-2 target, sole-selection toggle, no target → Trace("target none"). DoubleTap → 40289. `VerbDoubleTapKeys()` now {IN, OUT, ESC, TIMELINE}.
- Tests: `test_timeline_gestures` (7 scenarios) + updated `test_double_tap_keys` (4 keys). FakeProject does not apply selections — "state after tap" simulated by setting `ItemInfo.selected`.
Dead ends:
- First build (job `check-m5t5`) failed: composite rows are free functions in verbs.cpp's anonymous namespace and can't touch private `VerbRouter::select_delta_` → made it public. Committed as `31-m5-targeting: 5. select_delta_ public`.
- `check-m5t5b` failed verbs_test: FakeProject records but never applies SetSelectedItems, so consecutive hold+rotate steps always saw "nothing selected". Fixed in the test only: `ApplyLastSelection()` pushes the last recorded selection into `items[].selected` between gestures (REAPER applies it; the fake doesn't).
Facts (final):
- `tools/check verbs` green (job `check-m5t5c`): verbs_test 1/1, WINDOWS ORACLE PASS static, ORACLE PASS host, rc=0. Line 66 ticked; commits: `49cb928` (TIMELINE select verbs), `select_delta_ public`, test apply-selection fix.
- Design decisions for task 6/7: TIMELINE tap with no cursor target traces `target none` and does nothing; hold+rotate from an empty selection steps from an end (+ → first item, - → last item); multiple selected items on the track step from the leftmost.

Result: exit 0; done; log: logs/31-m5-targeting-L66-a5-1791854890180.log

### Done — 2026-10-13 — 5 attempt(s)
Files:
```
M vibes/.ralph/31-m5-targeting.md
```

## Task: 6. Oracle scenario: `target` (proves the ids and the behavior)

### Attempt 6 — 2026-10-13T01:34:40Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Tree was clean. KEY codes (src/speed_editor/key_map.h): TIMELINE=0x1B, RIPL DEL=0x2B; IN=0x07, OUT=0x08, TRIM IN=0x09, ESC=0x31. Taps on double-tap-set keys {IN,OUT,ESC,TIMELINE} are deferred 300 ms after release; RIPL DEL/TRIM IN fire at release.
- RiplDel composite (ripple off): 40310, 40006, 40309; tsel present: 40201 only. RippleMode uses GetToggleCommandState(40310/40311); setup guarantees off via Main_OnCommand(40309). Clear tsel via 40635.
- Created oracle/fake/target.txt + oracle/scenarios/target.env + target.lua (shape of verbs.*). Timeline (device ms): RIPL DEL 400, ESC x2 1600-1800, IN 2800, OUT 3400, RIPL DEL 4000, TRIM IN 4600, TIMELINE tap 5200/5900, TIMELINE x2 6600-6800; script clock = device - ~40 ms; script moves edit cursor to 1.0/2.0/0.5 before IN/OUT/TRIM IN. Added `target` to default suite in tools/reaper-oracle (list + header comment).
- Committed files before running (windows-oracle gotcha).
Next: run `tools/reaper-oracle --layer reaper --scenario target` (bg job), fix any failing step (record id corrections in docs/notes/m5-observations.md — note that file does NOT exist yet, create if needed).

Result: exit 0; progress; 4 files changed, 17 insertions(+), 20 deletions(-); log: logs/31-m5-targeting-L78-a6-1791855280919.log

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 6 attempt(s), last: exit 0, progress
Log: logs/31-m5-targeting-L78-replan-1791856776597.log

## Task: 7. SMOKE-TEST.md: "M5 — targeting"

### Attempt 1 — 2026-10-13T06:16:58Z — budget 1800s
- Facts: SMOKE-TEST.md has sections M1, M2, M3, M4 — the task says insert M5 "after M3", so it now sits between M3 and M4 (literal reading; M4 pre-dates the task text). TIMELINE is a real device key (0x001B, `src/speed_editor/key_map.h`). M5 composites per `docs/plugin-design-m5-m9.md` §1–§3 and `oracle/scenarios/target.lua`: RIPL DEL = time selection → 40201, else target delete 40006 inside temp per-track ripple (40310/40309); TRIM IN/OUT = 40511/40512 in WithTargetSelected; TIMELINE tap = exclusive select / re-tap deselects, double-tap = 40289, hold+rotate = 1 item per kSelectStepTicks=20, clamped, cursor untouched. New section has 5 steps + pass bar, each with an **Expected** line (grep -c 'Expected' = 39, verify exits 0).
- Dead ends: none.
- Next: none — box ticked; task done.

Result: exit 0; done; 9 files changed, 104 insertions(+), 85 deletions(-); log: logs/31-m5-targeting-L89-a1-1791872218837.log

### Done — 2026-10-13 — 1 attempt(s)
Files:
```
M SMOKE-TEST.md
 M oracle/lib.lua
 M oracle/scenarios/buttons.lua
 M oracle/scenarios/hid_wheel.lua
 M oracle/scenarios/shuttle.lua
 M oracle/scenarios/spike_rate.lua
 M oracle/scenarios/verbs.lua
 M vibes/.ralph/31-m5-targeting.md
 M vibes/31-m5-targeting.md
```
