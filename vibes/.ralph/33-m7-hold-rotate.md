# Ralph record: 33-m7-hold-rotate

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/check` cross-builds the Windows plugin via nix, and the flake’s `src = ./.` copy includes only git-tracked files. `git add` any newly created source before relying on `tools/check` (or it fails with “Cannot find source file”).


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
