# Backlog — milestones after M4 (NOT ready for ralph)

Outline of everything past `23-m4-shuttle.md`. These are goals, not sized
tasks. When M4 is done, turn the next section into its own
`vibes/NN-mX-*.md` file in ralph format (see `pi-ralph guide`): one artifact
per task, a Verify each, research/spike *first*, design settled in a `docs/`
file before implementation tasks. The first box below makes ralph stop here.

Already moved into M1–M4 (do not redo): HID integration, wheel modes JOG/SCRL,
STOP/PLAY, gesture engine (tap / double-tap / hold detection), tap-only verbs
(SPLIT, TRIM, RIPL DEL, IN/OUT, SNAP + LED, ESC/UNDO), shuttle law.
Still here from the old M3: hold+rotate verbs (SPLIT→MOVE, TRIM nudge), the
status-bar LEDs (ripple 3-state, trim-behind), APPND.

Oracle rule for every promoted section: each feature gets a `tools/reaper-oracle` scenario (fake device script + Lua assertions + `Trace()` lines, see `docs/plugin-oracle-design.md`) *before* the SMOKE-TEST.md hand-test task; the human only checks what the oracle cannot (feel, LEDs, audio).

Priority order for turning into files: **B1** (old M3 remainder) → **B2**
(old M4 crossfades/takes — takes+CAM LEDs first) → **B3** SOURCE focus →
**B4** configuration → **B5** composites. Windows builds stay out of scope.

- [!] Done
  BLOCKED: backlog only; a human (or planning model) promotes the next section to its own vibes file.

---

## B1 (old M3 remainder) — Core editing verbs + LED status bar (examples E3, E5)

Goal: the tape-mode loop of `reaper-workflow.md` §4 is runnable entirely on
the device, and the hidden-state footgun (3-state ripple) is made visible
on LEDs.

- Gesture engine v2: hold+rotate (hold threshold; wheel deltas while a key is held adjust in steps; release ends).
- Verbs (TIMELINE focus, workflow §7): SPLIT (+ hold→MOVE nudge), TRIM IN / TRIM OUT (+ hold+rotate = 1-frame edge nudge), RIPL DEL, IN / OUT = time selection start/end (+ double-tap = clear), APPND, SNAP toggle (+ LED).
- LED status bar: a timer loop reads REAPER state (snap, ripple 3-state, trim-behind) and drives SNAP/CUT/DIS/LIVE_OWR LEDs exactly per workflow §6 — the device reads REAPER state, never writes it (except its own SNAP button).
- Research: find the API-level *read* of the 3-state ripple + trim-behind + snap (workflow/reaper-workflow docs have the keymap + cmd 1155, but the LED loop needs an authoritative query, not a cached guess); verify each candidate in the app before coding.
- SMOKE-TEST.md task: add section "M3 — editing verbs & ripple LED": E3 (JOG to edit, SPLIT, hold-SPLIT rotate = MOVE, TRIM OUT hold+wheel), E5 (enable tape mode in REAPER — snap + ripple + trim-behind — and confirm all three LEDs light; flip `Alt+P` twice in REAPER and watch CUT/DIS follow within ~1 s; APPND chain; RIPL DEL closes the gap; ESC double-tap undoes). List expected results per step.

Completion criteria:
- **Ripple LED is always truthful** in both directions (change state on the
  device → LED; flip `Alt+P` in REAPER → LED follows), no more than ~1 s
  stale;
- every verb behaves exactly like its REAPER keybinding equivalent
  (SPLIT = `S`, RIPL DEL respects ripple state, TRIM = `Ctrl+[`/`]` at
  cursor semantics per workflow §5 targeting);
- hold+rotate nudge is quantized (1 frame / snap-quantized), one undo step
  per gesture (ESC double-tap restores the pre-gesture state).

Hand test (user): SMOKE-TEST.md "M3" section.

---

## B2 (old M4) — Crossfade family, roll/slip, takes & markers (examples E3 done, E4, E6, E8)

Goal: the transition family, the ROLL/SLIP family, take selection with CAM
LEDs, and the simple weak-label reinterprets.

- Crossfade family (workflow §7): TRANS, DIS, SMTH CUT (crossfade in time selection, `X`-equivalent), CUT (remove crossfade at cursor), TRANS DUR (tap = set to default length, double-tap SET = save current as default, hold+rotate = fine length set).
- ROLL (+ double-tap SLIDE), SLIP SRC, SLIP DEST (both with hold+rotate fine nudge), PLACE ON TOP.
- CAM 1–9 = set active take N on selected items; CAM LEDs light the active take (10+ takes → nearest + REAPER's own display, workflow §6).
- AUDIO LEVEL: tap = volume read-out in message box, double-tap MARK = project marker at cursor, hold+rotate = item volume nudge in dB steps.
- Weak-label reinterprets (single-action): VIDEO_ONLY = isolate track view, AUDIO_ONLY = solo selection's tracks, FULL VIEW = maximize arrange (workflow §9).
- Research: verify the API path for crossfade *length* at the cursor — which item/take fade fields to set on overlapping items, and how "default crossfade length" is read/written (REAPER pref vs. project info); confirm in the app before coding TRANS DUR.
- SMOKE-TEST.md task: add section "M4 — transitions, roll, takes": full E3 including SMTH CUT + TRANS DUR, E4 (ROLL shared-edge beat fix, SLIDE variant), E6 (three-take item, CAM 2 → LED, jog + switch takes), E8 (MARK at cursor). List expected results per step.

Completion criteria:
- SMTH CUT with a time selection over two overlapping items produces the
  same crossfades as pressing `X`; CUT removes them;
- ROLL moves the shared edge with total length unchanged (verify duration
  before/after); SLIP keeps position+length fixed, SLIP DEST keeps content
  fixed;
- CAM LEDs always show the active take of the selection (change take in
  REAPER's take lane → LED follows);
- solo/isolate are true toggles (re-tap restores), FULL VIEW maximizes.

Hand test (user): SMOKE-TEST.md "M4" section.

---

## B3 (old M5) — SOURCE focus: S/D source-edge editing (example E7)

Goal: the key design decision of workflow §2.2 — SOURCE focus = S/D
editing of the selected item — is usable end to end.

- TIMELINE / SOURCE focus buttons: switch focus, re-tap re-asserts (LED flash or equivalent), focus shown per workflow §2.3 (item flash/outline + different wheel behavior + message on switch).
- In SOURCE focus: wheel (JOG) scrubs the selected item's source offset (`I_STARTOFFS`); IN / OUT set source in/out (+ double-tap = restore full source); SLIP SRC nudges content (length & position locked); STOP/PLAY previews (play selection); SCRL/SHTL keep their TIMELINE meaning or are no-ops-with-flash — decide per workflow §2.2 and document in the SMOKE section.
- Research: verify the live behavior of source scrubbing via `GetSetMediaItemInfo_Value(I_STARTOFFS/…)`: does playback follow a moving source edge, and — critically — group the whole wheel gesture into **one undo step** (batch the info writes; confirm with Ctrl+Z that one press restores the pre-gesture edges).
- Research: pick the item-flash mechanism (temporary selection highlight vs. freeze color vs. brief item nudge) — verify which one reads as "this is the item I'm editing" without polluting selection (workflow §5: the device never silently changes selection).
- SMOKE-TEST.md task: add section "M5 — SOURCE focus": E7 verbatim (select item → SOURCE → wheel scrubs source, STOP/PLAY preview, IN/OUT trim source, double-tap IN/OUT restores, SLIP SRC hold+wheel, TIMELINE to go back), plus the undo-grouping check. List expected results per step.

Completion criteria:
- hand test E7 passes; the flash/outline appears on focus-in and is
  clearly the selected item;
- one full wheel-scrub gesture = one undo step (ESC double-tap / `Ctrl+Z`
  restores the pre-gesture source edges in one press);
- switching focus back and forth never leaves REAPER in a broken state
  (no stray selection changes, no stuck wheel mode).

Hand test (user): SMOKE-TEST.md "M5" section.

---

## B4 (old M6) — Configuration (L1 + L2, L3 optional)

Goal: every parameter in `docs/plugin-configuration.md` is reachable per
the 3-layer model; the OPTIONS-mode gesture and LED value bands from the
config doc's "still research" list are settled.

- L1: all defaults from the config doc's parameter inventory, in code; missing config = defaults (never a broken startup).
- Persistence: global `reaper.ini` section `SpeedEditor` via `SetExtState(…, persist=true)` for every L2 param (+ `weak.*` if L3 lands); state that survives restart today (focus/wheel mode) also moves here.
- L2: OPTIONS mode — hold ESC ≥1 s to enter (softkey buttons + LED value bands for the ~10 high-frequency parameters), exit on re-hold; every param editable, value shown in LEDs + message box, persists across REAPER restart.
- L3 (optional, may slip to v1.1): learn/remap for the 12 weak-label buttons via `PromptForAction`; re-tap = unassign; structurally excludes gesture/wheel/ESC/UNDO keys.
- Research: settle the LED value-band mapping per parameter group (config doc §4 "still research") — verify by hand that the bands are distinguishable at a glance before freezing them.
- Research: decide whether a REAPER-side config window is warranted if L2 feels cramped by hand test (default: no — the device is the interface).
- SMOKE-TEST.md task: add section "M6 — configuration": enter OPTIONS in ≤1.5 s, change shuttle max rate and double-tap window, re-run the M2 shuttle portion and feel the difference, restart REAPER → values persist; delete the `SpeedEditor` section from `reaper.ini` → all defaults restored; (if L3) remap one weak button, fire it, un-remap it. List expected results per step.

Completion criteria (config doc §4):
- with **zero config**, workflow examples E1–E8 all pass;
- OPTIONS reachable in ≤1.5 s; every L2 param editable; value readable in
  LEDs + message box; persists across REAPER restart;
- `reaper.ini` contains the `SpeedEditor` section; deleting it restores
  all defaults;
- (if L3) remapping one weak button changes only that button.

Hand test (user): SMOKE-TEST.md "M6" section, then re-run the M2/M3
sections with the changed parameters.

---

## B5 (old M7) — Composites & multicam (v1.1, last)

Goal: the `*`-marked composites and the signature multicam gesture. Nothing
here is needed for a daily audio edit — this is the "full Resolve feel"
layer.

- Research: REAPER 7 multitrack sync — is there an API to enable/inspect sync across items for SYNC BIN, or is it UI-only? If UI-only, decide: message "do it in the item properties" vs. drop SYNC BIN (document the decision).
- Research: LIVE O/WR feasibility — can REAPER switch the active take **during playback** (multitrack live switching)? If not, define the degraded behavior (switch on the fly at stop, or take-switch armed for the next playback) or drop the feature; document the decision either way.
- CLOSE UP: duplicate item under cursor to new track above (+ hold+wheel = nudge the copy's start edge).
- RIPL O/WR and SRC O/WR composites (workflow §7) — each must land as exactly **one undo step**.
- SYNC BIN + LIVE O/WR per the research outcomes above.
- SMOKE-TEST.md task: add section "M7 — composites & multicam": CLOSE UP (+ nudge), RIPL O/WR / SRC O/WR with the one-undo-step check, SYNC BIN (or the documented fallback), LIVE O/WR (or the documented degraded behavior), E6 extended with live take switching. List expected results per step.

Completion criteria:
- every composite = one undo step (ESC double-tap reverts the whole verb);
- SYNC BIN / LIVE O/WR ship per the documented research decision — a
  feature may be *dropped with a written reason*, but not left ambiguous;
- all of E1–E8 (final versions) pass in one session, in order, as the
  end-of-project regression.

Hand test (user): SMOKE-TEST.md "M7" section, then the full E1–E8 pass.
