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
