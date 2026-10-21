# Ralph record: 45-cam-n

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- REAPER Lua export quirks: no `AddEmptyItem`/`SetEditCursor`/`GetEditCursor`; use `AddMediaItemToTrack`+`AddTakeToMediaItem`, `SetEditCurPos(pos,false,false)`, `GetCursorPosition()`.
- Native REAPER has NO per-track "select item at edit cursor" action; mouse-cursor variants (40528/40529/40530) are no-ops headless.
- `B_UISEL` set/clear per item is isolated (no cross-track effect) and does not move the edit cursor (cam_probe, REAPER 7.81).


## Task: 1. Research: native candidates → `docs/notes/cam-n-select.md`

### Attempt 1 — 2026-10-21T01:27:32Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `oracle/scenarios/cam_probe.lua` added (not in REAPER_SCENARIOS); `tools/reaper-oracle --layer reaper --scenario cam_probe` passes on REAPER 7.81.
- Measured: B_UISEL set/clear on track-2 item leaves track-1 item's B_UISEL/B_SELECTED untouched; edit cursor stays at 1.0 (Decisions confirmed, no contradiction).
- Candidates: 40528/40529/40530 (mouse-cursor based) are no-ops headless; 40416 works headless but is exclusive select AND moves the cursor (1.0→0.0); 40417 no-op at cursor 1.0.
- No native per-track toggle exists → H1 = no, no alternative id. H2: cursor-at-item-end for a named track not probeable headless; step-2 unit test is the arbiter.
- Candidate table + measured facts + `## Holes` appended to `docs/notes/cam-n-select.md`.

Dead ends:
- `reaper.AddEmptyItem`, `reaper.SetEditCursor`, `reaper.GetEditCursor` are nil in this REAPER 7.81 Lua export ("attempt to call a nil value") — replaced with `AddMediaItemToTrack`+`AddTakeToMediaItem` / `SetEditCurPos` / `GetCursorPosition`.

Next:
- Task 2 (`CursorItemOnTrack`) can start; it is the arbiter for H2.

Result: exit 0; done; log: logs/45-cam-n-L30-a1-1792546052389.log

### Done — 2026-10-21 — 1 attempt(s)
Files:
```
M vibes/.ralph/45-cam-n.md
```
