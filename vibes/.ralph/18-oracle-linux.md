# Ralph record: 18-oracle-linux

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).
- `tools/check [regex]` works from the worker shell: no `cmake` on PATH, but the script falls back to `nix develop` itself. Build is incremental in `build/plugin`; re-runs take ~10 s.


## Task: 1. Plugin trace

### Attempt 1 — 2026-10-11T22:11:46Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Tree was clean before starting; wrote `plugin/src/trace.h/.cpp`, `plugin/test/trace_test.cpp`, added `trace.cpp` to `PLUGIN_PURE_SOURCES`, wired §2 messages + `SPEED_REAPER_TEST_CRASH=entry` abort into `ReaperPluginEntry` (all returns go through a `done(result)` lambda that traces `entry result=<n>`).
- `tools/check` has no `cmake` on the worker PATH; it auto-falls-back to `nix develop --command tools/check`, so run it via the script, not bare cmake.
- Trace design: env read per call (no caching), steady_clock since first call, mutex-guarded, `fprintf`+`fflush`; line = `<ms> <msg>\n`.
- `loadapi missing=<n>` uses the raw return value of `REAPERAPI_LoadAPI`; existing fail behavior (return 0) unchanged.

Dead ends:
- First run: `LineEndsWith` failed on the second trace line because it still had its trailing `\n` (only the first line was cut at `\n`). Fixed by popping the final `\n` before comparing.

Next:
- Done: `tools/check trace` green (log `vibes/.ralph/logs/check-trace2.log`), line 25 ticked.
