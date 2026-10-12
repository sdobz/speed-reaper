# Speed Editor → REAPER: Proposed Plugin Workflow & Mapping

Design proposal (task 3). Inputs: `speed-editor-workflow.md` (how a person
operates the device in Resolve), `reaper-workflow.md` (how a person operates
REAPER + label congruence table), `speed-editor-capabilities.md` (protocol /
keycode / LED inventory), `reaper-api-subset.md` (API mechanics). Parameter
research (learning, tuning, `Shift+?`) belongs to the next task; this
document proposes **behavior**, not final parameter values.

Design principle: **the labels on the hardware are fixed; the REAPER meaning
is ours.** A label stays "congruent" when a REAPER editor, hearing the label,
predicts the result without a manual. Where no congruent meaning exists, we
reinterpret the label toward REAPER idiom (S/D editing, time selection,
takes) rather than importing a video-only concept.

---

## 1. The state model: three independent axes

Everything the device can do is a function of three orthogonal states. The
plugin owns all of them; REAPER state (ripple, snap, edit mode) is *read and
displayed*, never silently changed (exceptions listed in §6).

| axis | values | set by | shown by |
|---|---|---|---|
| **Wheel mode** | JOG / SHTL / SCRL | SHTL / JOG / SCRL buttons | the 3 jog LEDs (report `0x04`), 1:1 with Resolve |
| **Focus** | TIMELINE / SOURCE | TIMELINE / SOURCE buttons | SOURCE button's… has no LED; state is shown by REAPER itself (see §2.3) |
| **Gesture** | tap / double-tap / hold+rotate | per-key software timer in the plugin | — (device sends held-keysets only, no auto-repeat, capabilities §3) |

Defaults on plugin startup: wheel mode **JOG**, focus **TIMELINE**. Both
persist across plugin restart (config file). `TIMELINE` tap while in TIMELINE
focus, and `SOURCE` tap while in SOURCE focus, act as *no-op with LED flash*
so focus is always re-assertable (mirrors Resolve's "press again to zoom"
being a focus button's secondary behavior — we use the re-tap as a safe
reset instead, §2.3).

The gesture engine (§4) is the only genuinely new mechanism: tap debounce,
double-tap window, hold detection, and "held key + wheel delta = adjust".
Timing values (double-tap window, hold delay) are **parameters for the
config task** — defaults proposed here: 300 ms double-tap window, 400 ms
hold threshold, single key at a time (a second key pressed during the window
cancels the pending double-tap).

---

## 2. Focus modes: what the wheel and edit buttons act on

### 2.1 TIMELINE focus (default, ~90 % of the time)

- Wheel target = **edit cursor on the arrange** (JOG/SCRL) or **transport
  play rate** (SHTL, §3).
- All edit/trim/transition/insert buttons act on the arrange via
  cursor + selection semantics (§5).
- IN / OUT = **time selection** start/end at the cursor (double-tap = clear;
  the CLR 2nd label is congruent: clearing the time selection).

### 2.2 SOURCE focus (reinterpreted — the key design decision)

REAPER has no source viewer or Source Tape (`reaper-workflow.md` §1).
Importing "jog a tape and insert clips" would make every label lie. Instead:

> **SOURCE focus = S-point (source edge) editing of the selected item.**
> REAPER's signature S/D (source–destination) editing is the congruent
> home: the "source" of an item is literally what REAPER calls *source*
> (the hidden content beyond the trim points).

In SOURCE focus, with one or more items selected:

- **Wheel (JOG)** scrubs the item's **source trim position** (equivalent to
  dragging the S-point; the item plays the newly revealed content). API:
  adjust `I_STARTOFFS` (source offset) on the active take, `GetSetMediaItemInfo_Value`.
- **IN / OUT** set the item's **source in / out** (trim start/end = the S
  edge), i.e. the same operations as TRIM IN/OUT but explicitly on the
  source side; **double-tap = clear** restores the full source (S=0 /
  end-of-media), congruent with the CLR label.
- **SLIP SRC** nudge the whole item's content (both S/D together, length
  fixed) — the label already *is* REAPER vocabulary.
- **STOP/PLAY** previews the revealed content (play selection).
- Insert verbs in SOURCE focus act on the selected item (e.g. PLACE ON TOP
  = copy it to a new track above).

**Deliberately not imported:** the Resolve "grab from media pool, mark
IN/OUT, insert into timeline" loop. The honest REAPER assembly workflow is
Media Explorer (mouse) + tape-style editing (ripple + trim-behind,
`reaper-workflow.md` §4) driven by the device. We say so in the manual
instead of faking a tape.

### 2.3 How the user *sees* the focus state

The SOURCE button is unlit (one of the 22 non-LED keys). Feedback comes from
REAPER itself: in SOURCE focus the plugin **flashes / outlines the selected
item's source edge** (e.g. brief item selection highlight or a temporary
item freeze-color) and the wheel's wheel-mode behavior is visibly different
(source scrub vs. cursor move). A config option (later) may reserve one
main-panel LED bit (candidate: `SMTH_CUT`, §6) for a persistent SOURCE-focus
indicator. Focus is also part of the text the plugin logs to REAPER's
action result / message box on every switch.

---

## 3. Wheel modes and the shuttle control law

| mode | protocol mode (capabilities §2.1) | REAPER behavior |
|---|---|---|
| **JOG** | `RELATIVE_0` | edit-cursor move. Small deltas = 1-frame (or snap-quantized) nudges; larger deltas proportional. While playing: also moves the playhead. API: `SetEditCurPos` (cursor, stopped) / seek via `SetEditCurPos(...,seekplay=true)` (playing) — `CSurf_OnSeek` is not exported and `CSurf_ScrubAmt` does not move a stopped cursor (docs/notes/oracle-limits.md). In SOURCE focus: source-edge scrub (§2.2). |
| **SCRL** | `RELATIVE_2` | **arrange view scroll only — cursor does not move.** `CSurf_OnScroll`. This differs from Resolve (where SCRL moves the playhead fast); in REAPER the cursor-move job is JOG+SHTL, so SCRL takes the one job REAPER has a first-class action for: `View: Scroll`. Enables the "travel far, land precise" pattern: SCRL across the project → JOG to settle. |
| **SHTL** | `ABSOLUTE_DEADZERO` | **shuttle — our own control law** (REAPER has no analog shuttle). Center (dead band) = stop; offset magnitude = rate; sign = direction. Implemented over playrate: `Master_NormalizePlayRate` maps 0.5 ↔ 1.0×, so wheel baseline (0) → normalize 0.5. |

**Shuttle law (proposed, all values are config parameters):**

- Dead band: |offset| < 40 (of the ±4096 range) → rate 1.0× stop (pause
  transport, do not rewind to center — the wheel is left where the user
  left it; STOP/PLAY is the explicit stop).
- Mapping: linear from 1.0× at the dead-band edge to **8.0× max** at full
  deflection, using a gentle curve (e.g. `rate = 1 + 7·(x/4096)²·(3−2x/4096)`
  smoothstep) so fine control sits near center. Reverse sign → reverse
  playback.
- Releasing the wheel does **not** reset it (matches the physical device:
  absolute position persists); the user parks it at center or presses
  STOP/PLAY. SHTL press re-centers the baseline (protocol mode reset) and
  flashes its LED.
- STOP/PLAY while shuttling: hard stop, playhead stays, wheel baseline
  unchanged. JOG press while shuttling: seamless handoff (rate → 1.0×,
  wheel mode → relative) — this is the Resolve "tap JOG to land" gesture.

While SHTL is active and transport plays, **follow-edit-cursor view**
should be on (plugin may toggle it on entry, restore on exit — or leave it
to the user; config parameter).

---

## 4. The gesture engine (plugin software behavior)

The device reports only *held keysets* (`0x04`), no auto-repeat, no
double-press detection (capabilities §3). The plugin implements:

1. **Tap** — keydown→keyup with no second press within the double-tap
   window → primary function fires **on keyup** (deferred, so a double-tap
   can still cancel it). Deferral latency (≤300 ms) is the cost of the
   grammar; primary functions must tolerate it (they all do — none is
   auto-repeating).
2. **Double-tap** — second press inside the window → secondary function
   (the 2nd label), the pending primary is cancelled.
3. **Hold + rotate** — key held ≥ hold threshold, then wheel deltas while
   held → analog adjustment in steps (step size = config parameter, often
   "1 frame" or "1 ms of length"). Key release ends the adjustment.
4. **Hold (no rotate)** — if a key has no hold+rotate function, holding
   does nothing extra. (Resolve's "hold + wheel on SPLIT = MOVE" is the
   canonical case.)
5. Wheel reports in JOG/SCRL are consumed by the wheel engine, not the
   gesture engine — except while a key is held in hold+rotate (held key
   wins).

---

## 5. Targeting: cursor + selection, with a nearest-edge rule

REAPER has no Smart Indicator. Decision: **stay REAPER-native** (cursor +
selected items), plus one plugin-implemented rule where it makes a label
honest:

- **Default:** all buttons act per REAPER semantics — at the **edit
  cursor** and/or on **selected items**, exactly as the corresponding
  keyboard action would. Example: SPLIT with nothing selected splits *all
  items on all tracks* at the cursor — same as pressing `S`. Editors
  learning the device learn REAPER, not a second mental model.
- **SMART INSRT** is the one exception: it inserts at the **nearest edit
  point** (item edge) to the cursor, biasing toward the *next* edge when
  equidistant, and applies ripple per the project setting (insert flag
  `&16384`, `reaper-workflow.md` §4). This is the closest honest
  translation of the Smart Indicator; it is also exactly what "smart
  insert" promises on the label.
- Selection discipline: the device does **not** change selection as a side
  effect (except insert verbs, which select what they created — REAPER
  default). If no item is selected and a button needs a target item, the
  plugin uses the item containing (or nearest to) the cursor and **flashes
  it**, never silently selects it.

---

## 6. Hidden state on the LED surface

18 main-panel LEDs (report `0x02`) are the status bar. Proposed assignment
(buttons whose function is momentary free their LED bit for state):

| LED (bit) | shows | encoding |
|---|---|---|
| SNAP (5) | magnet/snap | lit = on (1:1 with the button) |
| CUT (1) | **ripple: per-track** | lit = per-track |
| DIS (2) | **ripple: all-tracks** | lit = all-tracks (both CUT+DIS lit = all-tracks; both dark = off) |
| LIVE_OWR (9) | **trim-content-behind** on (tape mode) | lit = on — with per-track/all ripple this is full S/D "tape mode" |
| SMTH_CUT (3) | SOURCE focus (see §2.3; or leave dark) | optional, config |
| TRANS (4) | auto-crossfade preference on | lit = on |
| VIDEO_ONLY (13) | "audio-only" solo active (§8) | lit = selection-solo armed |
| AUDIO_ONLY (17) | "isolate view" active (§8) | lit = isolate on |
| CAM 1–9 (bits 6–8, 14–16) | **active take number of the selected items** (not "camera") | lit set = current active take; 10+ takes → nearest lit + REAPER take display |
| CLOSE_UP (0), SMTH_CUT if unused, TRANS if unused | reserved | — |

Jog LEDs (report `0x04`) stay 1:1 wheel mode, exactly as in Resolve.
Battery (report `0x07`) → REAPER message at low level (<20 %) and
"charging" in the startup log.

The ripple LED scheme exists because ripple is a **3-state hidden dial**
(`reaper-workflow.md` §1/§9): with it unlit, TRIM/RIPL DEL/MOVE silently
change behavior — the same footgun Resolve has, now with three states
instead of one.

Plugin rule: the device **never writes** REAPER state (ripple, snap,
trim-behind) except through its own buttons (SNAP toggles snap; the
composite "tape mode" gesture in §9 example 5 sets ripple+trim-behind).
State is *displayed*, changed on purpose, shown afterwards.

---

## 7. The full 43-button mapping

Primary = tap; secondary = double-tap or hold+wheel (2nd label). "—" = no
secondary. `*` = composite / later milestone.

| label | tap (primary) | 2nd label → secondary | notes |
|---|---|---|---|
| **STOP/PLAY** | transport play/stop toggle | — | thumb anchor, works in both focuses |
| **JOG / SHTL / SCRL** | set wheel mode (LED) | — | §3 |
| **TIMELINE / SOURCE** | set focus | re-tap = re-assert focus | §2 |
| **SMART INSRT** | insert selected/armed item at **nearest edit point**, ripple per project, select result | (CLIP on hardware) — no secondary in v1 | §5; "armed item" = selected item's SOURCE-focus region (§2.2) |
| **APPND** | seek to end of project, insert selected item, select result | (CLIP) — | `CSurf_GoEnd` + insert |
| **RIPL O/WR** * | overwrite the item at the cursor with the armed item, ripple neighbors to the new length | — | composite: overwrite + ripple-close |
| **SRC O/WR** * | overwrite the item under the cursor, **no** ripple (length change leaves gap/overlap, trimmed) | — | composite; advanced |
| **CLOSE UP** * | duplicate the item under the cursor onto a **new track above**, copy trimmed to the time selection if set | YPOS → hold+wheel = nudge the copy's start edge (trim) | REAPER idiom for "instant cut-in": overlay copy |
| **PLACE ON TOP** | copy selected item(s) to a new track above at the same position | — | the B-roll/cutaway verb |
| **IN / OUT** | TIMELINE: set time-selection in/out at cursor · SOURCE: set item source in/out | CLR → double-tap = clear time selection / restore full source | §2 |
| **TRIM IN / TRIM OUT** | trim start / end of selected (or cursor-containing) item to cursor | — | fine work: hold+wheel = edge nudge (1-frame steps, `Ctrl+,`-equivalent) — even though the 2nd label slot is free, we give both buttons hold+wheel nudge (most-used fine-adjust on the device) |
| **ROLL** | roll the boundary at the cursor: selected pair (or item at cursor + its neighbor) move shared edge toward cursor, total length fixed | SLIDE → double-tap = move the selected item along the timeline without changing neighbors' content (slide) | §9 example 4 |
| **SLIP SRC** | nudge selected items' source offset (content slides in item, length & position fixed) | — | already REAPER vocabulary; hold+wheel = finer slip |
| **SLIP DEST** | nudge selected items' position (both edges, content fixed) | — | hold+wheel = finer |
| **RIPL DEL** | remove selected (or cursor-containing) item + close gap (respects ripple state, §6) | — | |
| **SPLIT** | split at edit cursor (selected items, or all tracks if none selected — REAPER's own `S` semantics) | MOVE → hold+wheel = move the selected item(s) by nudge steps | |
| **TRANS** | create crossfade at the cursor (overlapping items: crossfade at that edge) | TITLE → double-tap+wheel: not used in v1 (no title objects) — reserved | REAPER has crossfades, not transition objects (`reaper-workflow.md` §6) |
| **DIS** | same verb as TRANS in v1 (dissolve = crossfade); kept distinct so future crossfade-shape options can diverge | — | |
| **SMTH CUT** | **crossfade within the time selection** (`X`-equivalent) — the jump-cut fixer; if no time selection, crossfade at cursor | — | |
| **CUT** | remove crossfade at the cursor (Restore/No crossfade at that edge) | — | "remove transition" stays congruent |
| **TRANS DUR** | set crossfade length at cursor to the default crossfade length | SET → double-tap = save current length as default · hold+wheel = set crossfade length at cursor (fine) | |
| **AUDIO LEVEL** | toggle a per-item gain nudge mode? **No:** tap = flash/confirm item volume in the message box (read-out) | MARK → double-tap = insert project marker at cursor (`M`) · hold+wheel = nudge selected item's volume (dB steps) | marker mapping is 1:1 congruent (`reaper-workflow.md` §7) |
| **SNAP** | toggle magnet/snap (LED) | — | 1:1 |
| **ESC** | cancel current gesture / clear time selection (REAPER's `Esc`) | UNDO → double-tap = undo (`Ctrl+Z`) | not a view-escape: REAPER has no device views to escape |
| **CAM 1–9** | set **active take N** on the selected items (LED lights the active take) | — | takes = multicam substitute (`reaper-workflow.md` §8) |
| **SYNC BIN** * | enable multitrack sync across the selected items (REAPER 7+; otherwise: message "needs REAPER 7") | — | later milestone |
| **LIVE O/WR** | arm/disarm **live take switching** (LED): while armed, jogging past item boundaries sets the active take of the held CAM button in real time | — | the signature multicam gesture, plugin behavior, later milestone |
| **VIDEO_ONLY** | **isolate view**: hide all tracks except those holding the selected items (restore on re-tap) | — | audio-DAW reinterpretation of the view-filter pair |
| **AUDIO_ONLY** | **audio-only: solo** — solo the tracks of the selected items (de-solo on re-tap) | — | "hear only this source" is the congruent reading |
| **FULL VIEW** | maximize the arrange window / toggle full-screen REAPER | — | |

Buttons marked `*` are composites (multi-step action sequences) — allowed by
the plugin's architecture (it calls REAPER actions/API, not just key
injection) but scheduled for later milestones; v1 ships everything without
`*` first.

---

## 8. Worked examples (button by button)

Every example assumes defaults (JOG mode, TIMELINE focus) unless noted, and
shows LEDs. "flashes" = plugin item-flash feedback (§5).

**E1 — Find a bad take in a long recording, land on the frame.**
SCRL (LED: SCRL). Spin the wheel: the arrange scrolls across the project,
cursor untouched. Tap JOG (LED: JOG). Small turns: cursor steps 1 frame at a
time (snap on → snaps to items). Stop. *Pattern: SCRL to travel, JOG to
land — the Resolve home pattern, preserved.*

**E2 — Shuttle through an hour of audio with motion.**
SHTL (LED: SHTL, baseline re-centered, wheel reports 0 → paused at 1.0×).
Turn right: transport runs forward, rate grows from 1× (smoothstep, §3);
turn left: reverse. Park wheel at center: stopped. STOP/PLAY: explicit stop
anytime. *The "wind" feel, built over playrate — REAPER's missing analog
shuttle.*

**E3 — Tighten a jump cut and smooth it (the daily edit).**
JOG to the edit. SPLIT (splits the item at the cursor; nothing selected →
all tracks, exactly like `S`). Hold SPLIT, rotate 15 steps left (MOVE 2nd
label): the right piece moves left, closing dead air (ripple per project
state, LED CUT/DIS shows which). Select the two meeting items, SMTH CUT
with a time selection over the cut: crossfade within selection (`X`). TRANS
DUR hold+wheel 3 steps: crossfade length set finer. TRANS DUR double-tap
(SET): that length becomes the default. ESC double-tap (UNDO) if any step
went wrong. *Every verb keeps its printed label; the 2nd-label grammar
works exactly as in Resolve.*

**E4 — Fix the beat of a boundary (roll).**
JOG to the boundary of two selected items. ROLL: shared edge moves so the
left item grows / right shrinks (or toward the cursor), total duration
unchanged — the beat lands. ROLL double-tap (SLIDE) instead, if you want
to move the whole clip along the timeline with neighbors rippling. ROLL
hold+wheel (via SPLIT-hold MOVE if you'd rather nudge): fine steps.

**E5 — Tape-mode assembly (the REAPER-native "source tape").**
One-time setup (also reachable as a composite gesture — config task): SNAP
on (LED), ripple per-track (CUT LED lit), trim-behind on (LIVE_OWR LED
lit) → **tape mode**, all three LEDs the status bar. Then: APPND the first
item (selected from Media Explorer, mouse), APPND the next, APPND the next.
Mid-sequence: TRIM OUT hold+wheel to shave 20 frames off the hidden tail —
content slides, everything after ripples closed. RIPL DEL on a dead item:
gone, gap closed. Wrong? ESC double-tap. *This is the classical
tape-style loop from `reaper-workflow.md` §4 — the honest substitute for
the Resolve Source Tape, with three-state ripple made visible.*

**E6 — Comp takes like multicam (CAM 1–3).**
Three takes per item (setup in the take lanes). Select the items to comp.
CAM 2: all selected items jump to take 2, CAM 2 LED lights. Jog the wheel
across the comp; at a boundary tap CAM 1, later CAM 3 — the active take LED
follows. (With LIVE O/WR armed, later milestone: hold CAM 3 while jogging
and take 3 is written in real time as you cross boundaries — the Resolve
live-override gesture, §8 of the REAPER workflow doc.)

**E7 — SOURCE focus: S/D editing of one item.**
Select the item. SOURCE (flash on its source edge). Wheel in JOG: the item's
source trim position scrubs — you hear the revealed content; STOP/PLAY to
preview. IN / OUT: set source in/out at the revealed position (double-tap
to restore full source, CLR). SLIP SRC hold+wheel: fine content-nudge with
length and position locked. TIMELINE: back to the arrange. *IN/OUT keep
their names because REAPER's source edge is genuinely called the source —
the label tells the truth in both DAWs.*

**E8 — Marker without leaving the device.**
JOG to the point. AUDIO LEVEL double-tap (MARK): project marker at cursor.
`[` / `]`… not on the device: hold AUDIO LEVEL rotate to nudge marker? —
out of scope v1; markers navigate by REAPER's own keys. (Kept simple
rather than overloading the button.)

---

## 9. What the labels that do NOT translate become (summary)

| label | Resolve meaning | proposed REAPER meaning | congruence |
|---|---|---|---|
| SOURCE / TIMELINE | focus source viewer / timeline | S/D (source edge) editing of selection / arrange | ⚠→✅ via reinterpretation (§2) |
| IN / OUT | tape in/out | time selection (TIMELINE) / source trim points (SOURCE) | ✅ (REAPER's own S/D vocabulary) |
| VIDEO_ONLY / AUDIO_ONLY | Cut-page view filters | isolate track view / solo selection tracks | ⚠→⚠ (weak, but honest for an audio DAW) |
| FULL VIEW / RVW | fullscreen review | maximize arrange | ⚠ |
| CAM 1–9 / SYNC BIN / LIVE O/WR | multicam cameras / sync bin / live cut | takes 1–9 / multitrack sync / live take switching | ⚠→✅ (REAPER's comp workflow is the analog) |
| CLOSE UP | punch-in track | overlay copy on new track above | ⚠ composite |
| RIPL O/WR / SRC O/WR / PLACE ON TOP | overwrite variants | composite insert verbs | ⚠ composite |
| TRANS / DIS / SMTH CUT / CUT / TRANS DUR | transition objects | crossfades (REAPER's one transition concept absorbs the family) | ✅ converged (§7) |

Bottom line: **every label remains predictable to a REAPER editor**; the
weak ones (video-view family, overwrite variants) are composites or
reinterpreted toward REAPER idiom and are the candidates for the
config/learning task to remap per user.

---

## 10. Out of scope here / deferred

- **Parameters** (double-tap window, hold delay, shuttle curve & max rate,
  nudge step sizes, ripple LED scheme toggle, SOURCE-focus LED): research
  task on plugin configuration (learning vs. static config vs. `Shift+?`
  style options).
- **Composites marked `*`** (RIPL O/WR, SRC O/WR, CLOSE UP, SYNC BIN,
  LIVE O/WR): later milestones.
- **Media-Explorer control** (wheel browses the pool): no first-class API
  found; revisit if the API task finds one.
- **Multiple devices / Bluetooth LE path**: single USB-HID device only.

## 11. Verification hooks for the manual smoke test

Each example E1–E8 above is directly hand-testable; concrete pass criteria
live in the milestone docs (task 5). Minimal v1 set: E1 (JOG/SCRL),
E2 (SHTL law), E3 (split/trim/roll/crossfade + UNDO grammar), E5 (ripple
LED truthfulness: flip `Alt+P` in REAPER and watch CUT/DIS LEDs), E6
(CAM→take LEDs), E7 (SOURCE focus IN/OUT/SLIP SRC).
