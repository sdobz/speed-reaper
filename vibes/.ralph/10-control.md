# Ralph record: 10-control

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `docs/reaper-plugin-workflow.md` (task 3) is the design contract for the
  plugin: 3-axis state model (wheel mode / TIMELINE-SOURCE focus / tap-
  double-tap-hold gesture), SOURCE focus = S/D source-edge editing of the
  selection (no fake Source Tape), SHTL shuttle law over playrate
  (deadzero, smoothstep to 8×), nearest-edge targeting only for SMART
  INSRT, LED status bar (ripple 3-state on CUT/DIS bits, trim-behind on
  LIVE_OWR, CAM LEDs = active take). Weak labels (VIDEO/AUDIO_ONLY, FULL
  VIEW, CLOSE UP, O/WR variants) are composites/reinterpreted. Parameters
  (timing, curve, step sizes) deferred to the config task.


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

Result: exit 0; done; log: logs/10-control-L5-a1-1791752698920.log

### Done — 2026-10-11 — 1 attempt(s)
Files:
```
M vibes/.ralph/10-control.md
```

## Task: Propose how the speed editor can be used, including mode switches, and keeping the labeled terms on the speed editor con

### Attempt 1 — 2026-10-11T21:11:38Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Wrote `docs/reaper-plugin-workflow.md` (~11 sections): state model, focus
  modes, wheel modes + shuttle control law, gesture engine, targeting
  rule, LED assignment table, full 43-button mapping (primary + 2nd-label
  secondary + REAPER action), 8 worked button-by-button examples (E1–E8),
  weak-label summary, deferrals, smoke-test hooks.
- Key design decisions: SOURCE focus reinterpreted as S/D source-edge
  editing (labels IN/OUT/SLIP SRC stay literally true in REAPER); SCRL =
  view scroll only (cursor untouched) so JOG stays frame-precise;
  SMART INSRT = nearest-edit-point insert (the one plugin-implemented
  Smart-Indicator analog); device never silently writes REAPER state —
  state is displayed on LEDs, changed via its own buttons.
- Ripple 3-state LED encoding: off = dark, per-track = CUT bit, all-tracks
  = CUT+DIS bits; trim-behind = LIVE_OWR bit ("tape mode" = all three on).

Dead ends:
- (none — pure synthesis of the two workflow docs + capabilities; no new
  web research needed)

Next:
- Task 4: plugin configuration research (learning? tuning? Shift+? options)
  — pick up every "config parameter" callout in reaper-plugin-workflow.md
  (§1 gesture timings, §3 shuttle curve/max rate, §4 step sizes, §6 LED
  scheme toggle, §2.3 SOURCE-focus LED).
- `docs/plugin-configuration.md` (task 4): config is 3 layers — L1
  compiled defaults, L2 on-device OPTIONS mode (hold ESC ≥1 s; buttons
  become softkeys, LEDs encode values; ~10 high-frequency params), L3
  learn restricted to the 12 weak-label buttons via `PromptForAction`
  (Resolve itself offers NO Speed Editor config, so everything is our
  design). Persistence = `SetExtState("SpeedEditor", …, persist=true)` →
  global reaper.ini; REAPER's own prefs absorb scrub/crossfade/snap
  tuning (no duplicate dials). Parameter inventory with defaults/ranges
  is the authoritative list for milestone docs.

Result: exit 0; done; log: logs/10-control-L7-a1-1791753098855.log

### Done — 2026-10-11 — 1 attempt(s)
Files:
```
M vibes/.ralph/10-control.md
```

## Task: Research how plugins are configured (learning? tuning? `Shift+?` options) - what parameters will the speed editor need?

### Attempt 1 — 2026-10-11T21:17:06Z — budget 600s

Facts:
- Resolve does NOT configure the Speed Editor at all: buttons are
  hardwired, no remapping (Blackmagic forum t=125672 + r/davinciresolve
  consensus); Resolve's only Speed-Editor-adjacent setting is app-side
  (e.g. transition duration via preferences). So all config is ours to
  design — no native behavior to inherit.
- Device has no SHIFT key and no options screen (43-key table checked,
  capabilities §3) — the task's "Shift+?" is hypothetical; the doc designs
  the on-device equivalent.
- `reaper-api-subset.md` has `PromptForAction` (native action picker,
  session_mode 1/0/-1) — the ready-made half of any learn/remap mode —
  and `SetExtState(persist=true)` → global reaper.ini, `SetProjExtState`
  → per-project. Chosen over a separate .cfg file.
- REAPER's own scrub tuning (Prefs → Playback) covers JOG feel — flagged
  as "no-new-param" rather than a device dial (reaper-plugin.md §2).
- Wrote `docs/plugin-configuration.md`: 3-layer model (L1 static
  defaults / L2 on-device OPTIONS mode = hold ESC ≥1000 ms, buttons as
  softkeys + LED value bands / L3 learn restricted to the 12 weak-label
  buttons via PromptForAction), full parameter inventory with defaults
  and ranges (~18 params, each marked L1/L2/L3/fixed), ExtState storage
  table, and task-5 completion-criteria candidates.

Dead ends:
- None — web research + local docs sufficient; no code needed.

Next:
- Task 5 (milestones): take `docs/plugin-configuration.md` §4 "v1 scope"
  and completion criteria as input; OPTIONS mode gesture (hold ESC 1 s)
  and LED value-band encoding are the two items still flagged as
  design-to-verify.
