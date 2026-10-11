# Ralph record: 10-control

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).


## Task: Research the davinci speed editor workflow in `docs/speed-editor-workflow.md`

### Attempt 1 — 2026-10-11T21:01:22Z — budget 600s

Facts:
- Wrote `docs/speed-editor-workflow.md` — end-user workflow research: Cut-page
  concepts (Smart Indicator, Source Tape, Ripple state), SOURCE/TIMELINE focus
  model, wheel JOG/SHTL/SCRL usage + protocol-mode choices, the tap/
  double-tap/hold+rotate input grammar with the full 2nd-label inventory
  (cross-checked against `bmd.py`/`key_map.h` annotations), assembly / trim /
  transition / marker / multicam (live override) workflows, LED & battery
  semantics, and 7 port constraints for the REAPER plugin (gesture engine,
  focus-vs-wheel-mode axes, smart-indicator targeting, ripple LED,
  absolute-deadzero shuttle law, LED status surface, auth/exclusivity).
- Key workflow source: captureguide.com guide (fetched OK); mixinglight 403'd
  (used search snippet only); ProVideo + Blackmagic pages via search snippets.
- Confirmed: double-tap and hold+rotate are Resolve *app* behavior — hardware
  sends held-keysets only (no auto-repeat, no double-press); our plugin must
  implement a per-key gesture engine.
- SHTL mode in the vendored demo = `ABSOLUTE_DEADZERO` (center=stopped,
  offset=speed) — natural shuttle control law for REAPER.

Dead ends:
- mixinglight.com article body: HTTP 403, only the search snippet usable.

Next:
- Task 2: `docs/reaper-workflow.md` (REAPER editing workflows to map onto).
