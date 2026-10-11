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

Result: exit 0; done; log: logs/10-control-L3-a1-1791752482924.log

### Done — 2026-10-11 — 1 attempt(s)
Files:
```
M vibes/.ralph/10-control.md
```

## Task: Research reaper editing workflows in `docs/reaper-workflow.md`

### Attempt 1 — 2026-10-11T21:04:58Z — budget 600s

Facts:
- Wrote `docs/reaper-workflow.md` — REAPER-side counterpart to
  `speed-editor-workflow.md`: editing model (arrange/edit cursor/items/
  takes), ripple is a 3-state dial (off/per-track/all-tracks, `Alt+P`,
  7.35 toolbar change, cmd 1155) + independent trim-behind toggle
  (`Shift+X`), edit modes, selection-centric action semantics, S/D
  editing, verified default keymap (S split, Tab transients, Ctrl+[ ]
  item edges, ,/. nudges, M markers, T/Shift+T takes, Alt+S snap, X
  crossfade, Alt+X auto-crossfade), CSurf API path for wheel modes
  (`CSurf_ScrubAmt`, `CSurf_OnPlayRateChange` + `Master_NormalizePlayRate`
  for the SHTL law, `CSurf_OnScroll`/`OnZoom`), no built-in analog
  shuttle in REAPER (community uses ShuttlePRO + playrate actions),
  transitions = crossfades only (no transition objects), multicam ≈
  takes + REAPER 7 multitrack, hidden-state/LED inventory, and a
  full device-label→REAPER-concept congruence table (✅ native / ⚠️
  reinterpret or composite).
- Biggest congruence gaps for task 3: no Smart Indicator (actions target
  cursor or selection, not nearest edge), no Source Tape (Media Explorer
  instead; IN/OUT ≈ time selection/trim points), SOURCE/TIMELINE focus,
  CLOSE UP / overwrite variants = plugin composites, ripple needs a
  3-value LED encoding.

Dead ends:
- `https://www.reaper.com/manual/` and `/manual` → 404 (no online manual
  there anymore); ask.video ripple article → 403. Used tutorialtactic
  cheat sheet + urosbaric series + airwiggles instead.

Next:
- Task 3: `docs/reaper-plugin-workflow.md` (mode switches + label
  congruence design; the congruence table in `reaper-workflow.md` §10 is
  its direct input). Verify the "(verify in app)" items during first
  smoke test: edit-mode list, Ctrl+drag-edge roll, Alt+X auto-crossfade,
  multitrack feature name.
