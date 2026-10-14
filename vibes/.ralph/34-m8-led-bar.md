# Ralph record: 34-m8-led-bar

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `GetToggleCommandState(id)` (0/1/-1) also works on REAPER's "Set ..." edit-mode commands (40309/40310/40311) — not only on "Toggle" actions (41990/41991, 1157, 41117). Fresh REAPER profile defaults: snap ON, trim-behind OFF, ripple off.


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
