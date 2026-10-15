# Ralph record: 42-release-prep

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- Milestone design docs (`plugin-design-m*`, `plugin-configuration`, `reaper-plugin-workflow`, `reaper-plugin`) live in `docs/archive/` now; so do the vendored `REAPER actions list.htm`, `reaper-api.htm`, `ReaperUserGuide781c/`. The current architecture doc is `docs/design.md`.


## Task: 1. Docs: keep what is still true, archive the rest

### Attempt 1 — 2026-10-15T17:38:40Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No tool or vibes file reads `REAPER actions list.htm`, `reaper-api.htm`, or `ReaperUserGuide781c/` → all three moved to `docs/archive/` too.
- `docs/design.md` written (288 lines ≤ 300) from current code: session.h, wheel_dispatcher.h, bindings.h, verb_registry.h, target.h, led_state.h, shuttle.h, main.cpp, default_bindings.ini; superseded bits (OPTIONS mode, SOURCE focus, ExtState) dropped, "why a text file" argued there.
- All 6 milestone docs + 3 vendored refs moved to `docs/archive/` via git mv; every `docs/<moved>.md` reference in vibes/41, plugin/, oracle/, SMOKE-TEST.md, docs/notes/, docs/*.md repointed to `docs/archive/...` (sed, per-file).
- Verify pattern (`rg 'docs/(plugin-design-m|…)'` excl. archive + vibes/[0-3]*) now matches nothing.
Facts:
- `tools/check` passed (job r42t1check, rc=0, ~18s warm); exact Verify command exits 0. Line 17 ticked; done.
Next:
- (task complete)

Result: exit 0; done; log: logs/42-release-prep-L17-a1-1792085921059.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
```
