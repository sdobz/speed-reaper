# Ralph record: 40-cleanup

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).


## Task: 1. Repo hygiene

### Attempt 1 — 2026-10-15T02:21:56Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - `oracle/scenarios/spike_xfade.lua` was ALREADY tracked (committed in 235b32a) — nothing to do there.
  - `.gitignore` already covered `oracle/__pycache__/` (via `__pycache__/`) and `build/oracle/` (via `build/`); added `vibes/.ralph-logs/` + `build/oracle/` explicitly anyway; `git rm -r --cached vibes/.ralph-logs` done.
  - The six probes are not in `REAPER_SCENARIOS` in `tools/reaper-oracle` (only `probe.json` output names match `probe_`). Deleted.
  - Docs citing probes, citation replaced by finding: `docs/notes/m4-shuttle-api.md` (scenario path → "since-deleted one-off scenario"), `docs/plugin-oracle-design.md` (scenario catalogue line → pointer to m4-shuttle-api.md), `docs/notes/m5-observations.md` (stripped probe names, findings stay), `vibes/32-m6-simple-verbs.md` (line 90), `vibes/23-m4-shuttle.md` (line 42). Also stripped two comment citations in `oracle/scenarios/smth_cut.lua` (in-suite scenario, would have been dangling). vibes/23 line 43/48 keep `--scenario spike_rate` as historical command invocations of a checked-off task, not path citations.
- Dead ends:
  - none so far
- Next:
  - DONE: `tools/check` green (job check40-1), full Verify rc=0, line 24 ticked, committed as `40-cleanup: 1. Repo hygiene`.

Result: exit 0; done; log: logs/40-cleanup-L24-a1-1792030916691.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
?? vibes/.ralph/40-cleanup.md
```
