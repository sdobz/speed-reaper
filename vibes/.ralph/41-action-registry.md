# Ralph record: 41-action-registry

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).


## Task: 1. `VerbRegistry` (pure)

### Attempt 1 — 2026-10-15T04:00:48Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; log: logs/41-action-registry-L52-a1-1792036848217.log

### Replan (depth 0) — SPLIT op=AND → "Registry core + 10 tap verbs + basic tests", "Turn verbs: select_step + nudge", "wheel + transport play_stop (shared WheelState)"
Trigger: 1 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Why: Attempt 1 made no progress at all: the worker read the code fine, then drafted the whole ~600-line design (14 verbs + core + tests) in its message and monologued to the 180s timeout without writing a file. The task is 2-3x the one-sitting size and leaves design details (gesture vocabulary, error shape, wheel-state ownership) open, which is exactly what sends this model into circles.
Rejected: BUDGET: no progress in the record (no files, no record bullets), so more time buys the same monologue. REWRITE: the task text and its Verify are sound — the problem is scope and unpinned design, not clarity, and a rewrite can't shrink it. SPIKE: no open question needs a doc; the milestone Design section plus the attempt's own monologue already settle the API, so a spike round-trip adds nothing. BLOCK: nothing needs a human decision.
Log: logs/41-action-registry-L52-replan-1792037167047.log
