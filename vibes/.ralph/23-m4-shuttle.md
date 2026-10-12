# Ralph record: 23-m4-shuttle

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- The Windows cross build inside `tools/check` snapshots the repo from git: new plugin files must be `git commit`ted before `tools/check` can see them (modified tracked files are seen, untracked files are not).


## Task: 1. `ShuttleRate` (pure)

### Attempt 1 — 2026-10-12T21:03:09Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Design §8 (docs/plugin-design-m1-m3.md L155): rate = 1 + 7·s²(3−2s), s = (|offset|−40)/(4096−40) clamped to [0,1], 0 in dead band |offset|<40, sign of offset.
- Created plugin/src/shuttle.h/.cpp (`double ShuttleRate(std::int32_t offset)`), added shuttle.cpp to PLUGIN_PURE_SOURCES (kept list alphabetical), added plugin/test/shuttle_test.cpp (test/CMakeLists globs `*_test.cpp` → ctest name `shuttle_test`).
- `cmake` is not on plain PATH; run everything through `tools/check` (it execs `nix develop`).

Dead ends:
- First `tools/check shuttle` run: Linux build + `shuttle_test` all passed, but exit 1 — the Windows cross build (`nix build .#reaper-plugin-windows`) packs a git-based source snapshot that includes *modified tracked* files but not *untracked* ones, so CMake found the new CMakeLists.txt (referencing shuttle.cpp) but not shuttle.cpp itself → "Cannot find source file". Fix: `git commit` the new plugin files (commit 9f9b726), then re-run.

Next:
- DONE: bgrun `check-shuttle2` exit 0 (shuttle_test passed, WINDOWS ORACLE PASS static, ORACLE PASS host). Line 21 ticked. Plugin files in commit 9f9b726.

Result: exit 0; done; 4 files changed, 57 insertions(+), 12 deletions(-); log: logs/23-m4-shuttle-L21-a1-1791838989818.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M SMOKE-TEST.md
 M tools/reaper-oracle
 M vibes/.ralph/23-m4-shuttle.md
 M vibes/23-m4-shuttle.md
 M vibes/30-backlog.md
?? docs/ReaperUserGuide781c/
?? docs/plugin-design-m5-m9.md
?? oracle/scenarios/ripple_del.lua
?? vibes/31-m5-targeting.md
?? vibes/32-m6-simple-verbs.md
?? vibes/33-m7-hold-rotate.md
?? vibes/34-m8-led-bar.md
?? vibes/35-m9-crossfade-spike.md
?? vibes/36-m10-bindings.md
```

## Task: 2. Spike actions: probe the playrate API

### Attempt 2 — 2026-10-12T21:07:38Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- API subset: `Master_GetPlayRate(ReaProject*)` (L108, pass nullptr), `Master_NormalizePlayRate(double, bool)` (L112, not needed), `CSurf_OnPlayRateChange(double)` (L268, projectless — sets current project's rate).
- main.cpp: added 4 spike actions (rate2=2.0, rate8=8.0, reverse=-1.0, reset=1.0+stop 40045), `REAPERAPI_WANT_CSurf_OnPlayRateChange` + `REAPERAPI_WANT_Master_GetPlayRate`, `SpikeState` global, timer prints `spike rate=%f read=%f state=%d pos=%f` (ShowConsoleMsg + Trace) ~300 ms after; reverse also prints pos0/pos1.
- Spike handling in onHookCommand2 placed before the `!g_engine` guard so probes work standalone; stop via Main_OnCommand(40045) (transport stop), play-start via 1007 per task.

Next:
- DONE: `tools/check` exit 0 (10/10 tests, WINDOWS ORACLE PASS static, ORACLE PASS host); verify command incl. `strings ... | grep -q speed_reaper_spike_reverse` passed; line 38 ticked; commit 426c46b (+ tick amend).
