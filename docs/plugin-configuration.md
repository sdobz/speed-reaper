# Research: how the Speed Editor plugin is configured

Answers task 4 of `vibes/10-control.md`: *how* is the plugin configured
(learning? tuning? `Shift+?`-style options?), and **which parameters** does
the speed editor need? Behavior contract: `reaper-plugin-workflow.md`;
device facts: `speed-editor-capabilities.md`; API facts:
`reaper-api-subset.md`.

## TL;DR

1. **There is no native configuration to inherit.** DaVinci Resolve ships
   the Speed Editor with a hardwired button layout and **no remapping or
   tuning UI** (Blackmagic forum + community consensus: the buttons cannot
   be remapped in Resolve; the only "settings" a Resolve user touches are
   app-side, e.g. the default transition duration the device applies).
   The device itself has no SHIFT key and no options screen — the "Shift+?"
   style menu mentioned in the task is a *hypothetical* we must design or
   reject.
2. **The printed labels are the strongest constraint.** 36 of 43 buttons
   carry printed labels that our mapping keeps congruent with REAPER
   (workflow doc §9). Full MIDI-learn-style remapping of all buttons would
   destroy the physical affordance the whole design leans on. Learn mode
   should therefore be **restricted to the weak-label / spare buttons**
   (VIDEO_ONLY, AUDIO_ONLY, FULL VIEW, SYNC BIN, CLOSE UP, CAM 1–9).
3. **Recommended configuration model — three layers:**
   - **L1 Static defaults**: sane values compiled in; user does nothing.
   - **L2 On-device OPTIONS mode**: our "Shift+?" equivalent — a
     gesture-entered mode where the button bank becomes softkeys and the
     LED bank shows current values, covering the ~8 high-frequency
     parameters. No screen exists, so values are encoded in LED patterns.
   - **L3 Learn/remap**: for weak-label buttons only; REAPER's own
     `PromptForAction` API (in our API subset) supplies the action picker,
     no custom UI needed.
4. **Persistence**: one global ini section via
   `SetExtState("SpeedEditor", key, value, persist=true)` (lands in
   `reaper.ini`, survives restarts, per REAPER convention); optional
   per-project overrides via `SetProjExtState` for project-specific things
   (e.g. SHTL follow-cursor behavior). Not a separate `.cfg` file — ExtState
   is the idiomatic plugin mechanism in the vendored SDK and keeps one
   backup unit (the user's REAPER config) as the single source of truth.

## 1. How things like this are usually configured (findings)

### 1.1 The reference product: Resolve + Speed Editor = fixed layout

- Resolve's Cut page consumes the Speed Editor verbatim. Buttons, their
  double-tap second labels, and hold+rotate adjustments are **hardwired in
  the app**; community asks for remapping are unanswered (Blackmagic forum
  "Speed editor remapping?", r/davinciresolve "Remapping the speed editor" —
  consensus: not possible, and a common complaint).
- What *is* tunable in Resolve is on the **application side, not the
  device**: e.g. the transition duration that TRANS applies is set in
  Resolve preferences (forum: "change standard transition duration … can
  currently only be done on the cut page / in preferences").
- Takeaway: even the reference product answers "tuning?" with *"the DAW's
  own preferences"*. For several of our parameters the right answer is the
  same — **reuse REAPER's existing preferences** (see §3, "no-new-param"
  column) instead of inventing device-side dials.

### 1.2 Learn mode (MIDI-learn convention)

Every DAW's controller-mapping UX is the same shape: enter learn/mapping
mode, pick the target (or press the control, then pick the target), done.
Two target-first / control-first variants, both well understood.

Important REAPER-specific fact: REAPER has built-in **MIDI learn** for
control surfaces, but the Speed Editor is **HID, not MIDI** — our plugin
owns the input path end-to-end, so REAPER's learn UI never sees our keys.
Any learn mode is ours to build. The good news: `PromptForAction`
(reaper-api-subset.md) gives us REAPER's native action picker
(session_mode=1 → poll → -1), which is exactly the "pick the target" half of
learn mode, in REAPER's own chrome.

### 1.3 Static config conventions in the REAPER ecosystem

- REAPER plugins persist settings with `SetExtState(section, key, value,
  persist=true)` (global `reaper.ini`) or `SetProjExtState` (per project);
  SWS/S&M do the same at scale. Some older plugins write their own
  `*.cfg` next to the exe (`GetAppDirectory`), but ExtState is the current
  idiom and integrates with REAPER's config backup.
- Tunable *behavior* often lives in REAPER's own Preferences instead of
  plugin config: the built-in scrub path ("Transport: Scrubbing fine
  control") is tuned under Preferences → Playback — reaper-plugin.md §2
  notes this as the option-A path. Principle: **if REAPER already has a
  knob for the same behavior, don't add a second one.**

### 1.4 On-device options ("Shift+?" equivalent)

There is no screen, no SHIFT key, and only 21 LEDs (18 main-panel bitfield
+ 3 jog-mode LEDs). But the plugin owns **both** input and output: an
options mode entered by a composite gesture can repurpose the 43 buttons as
softkeys and encode option values in LED on/off buckets. Precedent for
"buttons become menus while in a mode" exists in control-surface
convention (Mackie/TCG surface "Setup" keys). It is the only way to tune
while hands stay on the device — which is the whole point of owning the
device.

## 2. The parameter inventory

Grouped by where the user would touch it. "Layer": L1 = compiled default,
L2 = exposed in on-device OPTIONS mode, L3 = learn/remap target, — =
deliberately **not** a parameter (fixed by design, see note).

### 2.1 Gesture engine (workflow doc §4)

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `gesture.double_tap_window_ms` | 300 | 150–600 | L2 | Also sets single-tap **deferred-firing latency** — raising it feels safer, slower. The one knob users actually want. |
| `gesture.hold_threshold_ms` | 400 | 250–800 | L2 | Hold+rotate must not fire on accidental press; 400 ≈ "deliberate". |
| `gesture.cancel_on_second_key` | on | on/off | — | Fixed: a second key inside the window cancels the pending double-tap (workflow §1). |

### 2.2 Wheel: JOG

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `jog.step_mode` | snap-quantized 1 frame | frame / sample / free | L2 | Small deltas = 1-frame (or snap-quantized) nudges (workflow §3). |
| `jog.seconds_per_rev` | 20 | 5–60 | L2 | Timeline seconds per full wheel revolution (1,474,560 units, measured). Position-style 1:1 mapping, no smoothing. Basis: `docs/notes/m1-wheel-scale.md`. |
| `jog.respond_to_reaper_scrub_prefs` | on | on/off | L2 (toggle only) | Reuse **REAPER's** Preferences → Playback scrub tuning (reaper-plugin.md §2) instead of re-implementing speed. Off = our own fixed scale. |

### 2.3 Wheel: SHTL shuttle law (workflow §3 — all values named as config parameters there)

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `shuttle.deadband` | 40 (of ±4096) | 10–150 | L2 | Below this the wheel "rests" → 1.0×/stop. |
| `shuttle.max_rate` | 8.0× | 2–32 | L2 | Full-deflection rate; REAPER caps useful rates well below the 8.0× the protocol allows, keep default conservative. |
| `shuttle.curve` | smoothstep | linear / smoothstep | L2 | Fine control sits near center with smoothstep. |
| `shuttle.follow_cursor_view` | auto (on in SHTL, restore on exit) | on / off / auto | L2 | Workflow §3: "leave it to the user; config parameter." |

### 2.4 Wheel: SCRL

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `scroll.lines_per_notch` | 1 arrange line | 1–16 | L2 | `CSurf_OnScroll` granularity; "travel far" pace (workflow §3). |

### 2.5 Hold+rotate analog adjustments (workflow §4.3)

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `adjust.trim_step` | 1 frame | frame / sample / 10 ms | L2 | TRIM IN/OUT, IN/OUT, TRANS DUR hold+rotate granularity. |
| `adjust.move_step` | 1 frame | frame / 1/32 frame | L2 | SPLIT-hold (MOVE) nudge size. |
| `adjust.quantize_to_snap` | on | on/off | L2 | Hold+rotate follows snap grid or free steps. |

### 2.6 Focus / LED scheme (workflow §2.3, §6, §8)

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `led.source_focus_bit` | off (REAPER-side feedback only) | off / SMTH_CUT(3) | L2 | Workflow §2.3: "config option may reserve one LED bit (candidate SMTH_CUT)". |
| `led.ripple_encoding` | 3-state classic | classic / simple | L2 | Classic: CUT=per-track, DIS=all, LIVE_OWR=trim-behind (workflow §6). Simple: one LED on/off for "any ripple". Classic is the default — the LED status bar *is* the device's way of telling you the edit will ripple. |
| `led.battery_low_pct` | 20 | 5–50 | — | Fixed; warning threshold only (capabilities §4). |
| `led.cam_leads_take` | on | on/off | — | CAM LEDs = active take of selection (workflow §6); off only if the user ignores takes entirely — not worth an option, keep fixed for v1. |

### 2.7 Composite behaviors / one-time setups

| Parameter | Default | Range | Layer | Notes |
|---|---|---|---|---|
| `tape.mode_gesture` | (unassigned; mouse-only setup for v1) | assign composite | L3-era | Workflow E5: "also reachable as a composite gesture — config task." Deferred: composite-gesture authoring is a milestone item, not v1. |
| `weak.vid_only` … `weak.cam9` | weak-label defaults from workflow §9 | any action | **L3 (learn)** | The remappable set: VIDEO_ONLY, AUDIO_ONLY, FULL VIEW, SYNC BIN, CLOSE UP, CAM 1–9 (12 buttons). Everything else — including ESC double-tap = UNDO, the wheel-mode keys, IN/OUT — is fixed to its label. |

### 2.8 "No-new-param" column (deliberately reused from REAPER)

| Behavior | REAPER's existing knob |
|---|---|
| Fine scrub feel (stopped, JOG) | Preferences → Playback → scrub settings (the built-in "Scrubbing fine control" path) |
| Crossfade defaults that CUT/TRANS/DIS write | Item → Crossfade settings / Preferences |
| Snap grid that JOG/hold+rotate quantize to | View → snap config (the SNAP button only toggles enable, workflow §7) |
| Undo granularity of device edits | REAPER's normal undo history (ESC double-tap = one Undo step) |
| Transition duration TRANS applies | Default crossfade setting (the exact analog of Resolve's pref-side tuning, §1.1) |

## 3. The recommended configuration model

### 3.1 L1 — defaults

All table values above ship as compiled defaults; the plugin is fully
usable with zero configuration. Every parameter is read once at startup
(from ExtState or default) into a struct; OPTIONS mode edits write back
through `SetExtState(..., persist=true)`.

### 3.2 L2 — on-device OPTIONS mode (the "Shift+?" equivalent)

**Entry**: hold **ESC ≥ 1000 ms** (well above the 400 ms hold threshold so
it can't be an accidental hold+rotate), then release. Exit: tap ESC.
Rationale: ESC is the device's "out of trouble" key (UNDO lives there),
it never fires during editing grammar, and holding it is unmistakably
deliberate. Entry also **suspends the gesture engine's deferred primary
fires** so no pending action leaks out on exit.

**While in OPTIONS mode** (LED announcement: all 18 main LEDs cycle on/off
once on entry — a pattern no state bar ever produces):

- **SHTL / JOG / SCRL** buttons: select parameter *group* (gesture / wheel /
  focus-LED, one group per jog-mode key — 3 groups cover §2.1–2.6).
- **TIMELINE / SOURCE** buttons: step to previous / next parameter in the
  group.
- **Wheel (any mode)**: change the selected parameter. Coarse: one detent
  step. **Hold CUT + wheel**: fine step (e.g. 1 ms on timing params).
- **LED value encoding**: the 3 jog-mode LEDs show the value band
  (low/mid/high of the parameter range); the parameter's *own* panel LED
  (if it has one, e.g. CUT for the ripple encoding) mirrors on/off state.
  The message box / action-result text prints the exact value on every
  change (REAPER's log is the "screen" we don't have — same pattern the
  workflow doc already uses for focus switches, §2.3).
- **SNAP**: apply & exit (writes ExtState, flashes its LED). **SPLIT**:
  cancel & exit (no write).
- Re-entering OPTIONS later: wheel-mode LED group + value band make the
  current selection visible in < 1 s.

Scope guard: OPTIONS mode exposes **only** the L2-marked parameters
(≈10). If it grows past ~12, promote the overflow to a REAPER-side config
window (custom `IReaperControlSurface` option page or a simple
`ShowConfig` dialog via `ShowMessageBox`/`PromptForAction`) rather than
overloading the LED encoding.

### 3.3 L3 — learn / remap (weak labels only)

1. OPTIONS mode → hold **SOURCE** (the "assign" key in this mode) to open
   the remap target list; wheel steps VIDEO_ONLY … CAM 9; **TIMELINE**
   confirms the selection (its panel LED stays lit = "this button is in
   learn").
2. Tap the chosen physical button → `PromptForAction(1)` opens REAPER's
   action picker → poll until an action is chosen → stored as
   `weak.<key>` in ExtState (action ID string).
3. Re-tap the button while still in learn → unassign (falls back to the
   default from workflow §9).
4. Safety: only the 12 weak buttons accept remaps; the gesture engine,
   wheel modes, and ESC/UNDO are structurally excluded (remap table is
   keyed, not arbitrary).

Why not full-surface learn: the printed labels *are* the manual; an editor
who has learned "CUT = cut at cursor" must never have to relearn which
button that was after a config file hiccup. Weak labels are already
composites/reinterpretations (workflow §9's ⚠ rows) and are where
individual editors genuinely differ.

### 3.4 Storage summary

| Store | Contents | API |
|---|---|---|
| Global, `reaper.ini` section `SpeedEditor` | every L2 parameter + `weak.*` remaps | `SetExtState("SpeedEditor", …, persist=true)` |
| Per-project (optional, v1.1) | `shuttle.follow_cursor_view` | `SetProjExtState` |
| In-memory only | current group/param selection, pending tap state | — |

Defaults ship in code; missing ExtState keys → default, so a partial or
old config never breaks startup (same rule as `GetExtState` returning
`""` for missing keys).

## 4. What this answers for the milestone task (task 5)

- **v1 scope**: L1 defaults + L2 OPTIONS mode with the 10 parameters above.
  L3 remap can land in v1 *or* slip to v1.1 — it is 12 `PromptForAction`
  round trips over an existing code path.
- **Completion criteria candidates**: (a) with zero config the workflow
  doc's E1–E8 all pass; (b) OPTIONS mode reachable in ≤ 1.5 s, every L2
  param editable, value audible in LEDs + visible in message box, persists
  across REAPER restart; (c) `reaper.ini` contains the `SpeedEditor`
  section and deleting it restores all defaults; (d) remapping one weak
  button changes only that button.
- **Still research, not design**: exact LED value-band mapping (depends on
  which params land in each group once groups are settled); whether a
  REAPER-side config window is warranted if L2 exceeds scope; composite
  gesture authoring for tape mode (E5) — needs the gesture engine to
  exist first.

## Sources

- Blackmagic forum — "Speed editor remapping?" (no remapping in Resolve):
  https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=125672
- r/davinciresolve — "Remapping the speed editor" (community consensus it's
  not possible):
  https://www.reddit.com/r/davinciresolve/comments/1b477ld/remapping_the_speed_editor/
- REAPER SDK (vendored) — `PromptForAction`, `SetExtState`/`SetProjExtState`
  semantics: `docs/reaper-api-subset.md`; REAPER scrub fine-control
  preference: `docs/reaper-plugin.md` §2.
