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
