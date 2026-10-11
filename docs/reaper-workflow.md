# REAPER — How an Editor Actually Uses It (Editing Workflow)

`speed-editor-workflow.md` documents how a person operates the Speed Editor
with DaVinci Resolve. This document is the REAPER counterpart: how an editor
actually operates REAPER, the state the workflows depend on, and which
REAPER concepts are congruent to the device's labeled buttons (and which
are not). It is the other half of the input for the mapping design in
`reaper-plugin-workflow.md` (task 3). `reaper-api-subset.md` covers the API
mechanics (function/action names); this document covers *workflows*.

Sources: the REAPER action set / default keymap (verified against the
cheat-sheet in Sources below), hands-on editing write-ups (Uros Baric's
classical-editing series), the REAPER 7.35 ripple-mode change discussion,
and the API subset already researched in this repo. Facts marked *(verify
in app)* are standard REAPER behavior the sources did not spell out; check
them during the first manual smoke test.

---

## 1. The big picture: REAPER's editing model

REAPER is a non-linear DAW, not a Cut page. The mental-model differences
that drive every mapping decision:

| Resolve Cut-page concept | REAPER equivalent | Consequence for the port |
|---|---|---|
| **Timeline** | **Arrange view** — tracks × time; media items live on tracks | The wheel and all edit buttons act on the arrange. There is no second "page". |
| **Smart Indicator** (nearest edit point) | **None built in.** The closest primitives: "Go to next/previous edit" (item edges), transient jumps (`Tab` / `Shift+Tab`, nearest = `Ctrl+Tab`), zero-crossing jumps (`Z` / `Shift+Z` / `Alt+Z`) | Resolve's edit buttons target *the clip nearest the indicator*; REAPER's actions target *the edit cursor* or *the selected items*. The plugin must either (a) stay cursor-centric (REAPER-native feel) or (b) implement nearest-edge targeting itself and route button actions through it. This is the single biggest congruence gap. |
| **Source Tape** | **None.** The source browser is the **Media Explorer** window (View menu) — a file/take browser, not one continuous tape | There is no "jog the tape, mark IN/OUT, insert" loop in the native sense. The closest loop: Media Explorer → insert at edit cursor, or open a file as an item and trim it. IN/OUT on the device map more naturally to *time selection* or *item trim points* than to a tape. |
| **Ripple state** (on/off) | **Three states: off / per-track / all-tracks** (`Alt+P` cycles; toolbar button cycles too). Plus an *independent* toggle, "trim content behind items when editing" (`Shift+X` removes the hidden content), plus ripple markers/regions settings | Ripple is still hidden state, but it's a 3-state dial, not a switch — the LED scheme must encode three states (e.g. off = dark, per-track = dim/solid, all-tracks = blinking or brighter). Changed again in REAPER 7.35 (toolbar button behavior; action "Options: Cycle ripple editing mode", command 1155). |
| — (no equivalent in Resolve) | **Edit modes** (Options menu → Edit mode): Default, Item editing, Ripple, Trim, Nudge, MIDI *(verify in app)* | REAPER has a *mode axis on top of state*: the same mouse gesture (drag item edge) trims in default mode but does different things in ripple/trim/nudge modes. A plugin LED surface should show the active edit mode, or the device should never leave it in Default. |
| — | **Selection-centric actions.** Most actions act on *selected items*, not "the item under the playhead". Split `S` is the notable one: splits selected items at the edit cursor, or **all items on all tracks** if nothing is selected | Every button mapping must be specified as "cursor + selection semantics", not just an action name. |

**Time selection** is the workhorse the Cut page doesn't have: set a
region, play only it, crossfade within it, ripple-delete within it.
`Shift+←/→` grows it from the cursor; `Esc` removes it (Esc in REAPER is
"clear time selection", *not* an undo or escape — the device's ESC/UNDO
double-tap convention stays, but single-tap ESC should map to
clear-time-selection or a plugin-defined "cancel current gesture", not to
Resolve-style view-escape).

**S/D (source–destination) editing** is REAPER's signature technique:
you always manipulate *both ends* of an item — the source edge (what the
item shows) and the destination edge (where it sits). With ripple +
trim-behind on, dragging a middle item's edge behaves like tape editing
(content slides in/out, neighbors ripple). Classical editors build whole
workflows on this (Uros Baric series, Sources); the Speed Editor's
SLIP SRC / SLIP DEST labels are *already congruent* to REAPER's
source/destination vocabulary — keep them.

---

## 2. Playhead, transport & the shuttle reality

- **Play/Stop** = `Space` (or Media Keyboard keys). **Play/Pause** = `Enter`.
  **Repeat (loop)** = `R` — the loop region is the time selection or loop
  points; "play the section again" is the daily-use workflow.
- **Edit cursor nudge**: arrows (with snap), `Ctrl+←/→` for larger jumps;
  `W` = start of project, `End` = end of project.
- **Edit-point navigation** (the Smart-Indicator stand-in editors use):
  `Ctrl+[` / `Ctrl+]` = cursor to previous/next **item edge**;
  `Tab` / `Shift+Tab` = next/previous **transient**; `Z` / `Shift+Z` /
  `Alt+Z` = zero crossings.
- **Nudge dialog** = `N`; quick nudges: `,` / `.` (cursor or items),
  `Ctrl+,` / `Ctrl+.` (nudge item **left edge**), `Ctrl+Alt+,` / `Ctrl+Alt+.`
  (nudge item **right edge**), `<` / `>` (shift by time-selection length).
  This is how editors do frame-accurate trims by keyboard — and it is
  exactly the "hold key + wheel" analog-adjustment pattern the device
  provides.
- **Shuttle: REAPER has no built-in analog shuttle.** Community setups use
  ShuttlePRO-style hardware or custom playrate actions (Sources, §3). For
  the plugin this is good news: the SHTL wheel mode (absolute dead-zero:
  center = stopped, offset = speed, sign = direction) is *entirely our own
  control law*, implemented over `CSurf_OnPlayRateChange` / playrate APIs
  (`Master_NormalizePlayRate` maps playrate ↔ 0…1 slider position — i.e.
  wheel-center → normalized 0.5 ↔ 1.0×). JOG mode maps to
  `CSurf_ScrubAmt` (scrub while paused) / cursor nudges; SCRL to
  `CSurf_OnScroll`. All of these are first-class CSurf API calls
  (`reaper-api-subset.md` §5) — the plugin registers as an
  `IReaperControlSurface` and the transport behaves like REAPER's own
  control-surface transport.

---

## 3. The mouse editing model (what "feels like REAPER")

Default mouse behavior on items (reassignable via mouse-modifier prefs —
the API exposes this as `GetMouseModifier`/`SetMouseModifier`, §6 of the
API subset, contexts like `MM_CTX_ITEM`):

| gesture | default action | device-congruent name |
|---|---|---|
| drag item body | **Move** (slide; neighbors ripple only if ripple is on) | SPLIT-hold "MOVE" label stays congruent |
| **Alt** + drag item body | **Slip** (change source position, length & place fixed) | SLIP SRC |
| drag item **edge** | **Trim** (source edge; content beyond is hidden, not deleted) | TRIM IN / TRIM OUT |
| **Ctrl** + drag item edge | **Roll** (move the shared boundary of two items, both change, total fixed) *(verify in app)* | ROLL |
| click item edge (ripple off) | select | — |
| **Alt** + click item (or razor-area click, reassignable) | **Ripple delete** the item | RIPL DEL |
| drag in empty arrange | time selection | — |

Note the ripple dependency: the same "drag body" gesture slides or ripples
depending on the hidden 3-state ripple mode — same footgun as Resolve's
Cut page, now with three states.

**Split** is the workhorse: `S` at edit cursor (selected items, or all
items on all tracks if none selected), `Shift+S` across time selection,
`Ctrl+Shift+S` ignoring grouping, `Alt+Shift+S` at play cursor.

---

## 4. Workflow: assembly (getting material into the timeline)

REAPER-native assembly loop (no Source Tape — this is the honest version):

1. **Media Explorer** (View menu): browse files/takes; play/preview; drag
   to arrange at the edit cursor, or "insert at edit cursor" action.
2. **Tape-style editing** for linear assembly (the classical workflow):
   enable **ripple (per-track or all)** + **trim-behind** (`Shift+X`
   removes hidden content after edits), then the arrange behaves like tape:
   trim an edge → content slides in; delete middle material → everything
   after ripples closed; `S` splits; `Alt+P` switches scope.
3. Insert-verb equivalents for the device's insert cluster:

| Device button | REAPER semantics to implement |
|---|---|
| **SMART INSRT** | insert at edit cursor, applying project ripple setting (insert API flag `&16384` = "apply ripple according to project setting", API subset §8). If we implement nearest-edge targeting (gap in §1), this becomes "insert at nearest edit point". |
| **APPND** | seek to end of project (`CSurf_GoEnd` / "Go to end of project") then insert — or the "append item to end of project" style action. |
| **PLACE ON TOP** | insert on a **new track above** the selected track at the cursor (REAPER has this as an insert option; no single default key). |
| **RIPL O/WR / SRC O/WR / CLOSE UP** | no single-key REAPER equivalent; these become plugin composite actions (ripple-overwrite = overwrite + ripple close; close-up = duplicate item on new track + zoom/trim). Design belongs to task 3. |

---

## 5. Workflow: refinement (timeline edits)

| Device label | REAPER action(s) | Notes |
|---|---|---|
| **SPLIT** | "Items: Split items at edit cursor (or selected items if selected)" (`S`) | Congruent. Hold+wheel "MOVE" = drag-item action or move-by-nudge. |
| **TRIM IN / TRIM OUT** | "Trim item start/end to edit cursor" actions; fine adjustment via edge nudges `Ctrl+,` / `Ctrl+Alt+,` | Congruent if targeting is cursor-centric. |
| **ROLL** | roll edit (Ctrl+drag edge, §3) or nudge both edges of the pair by ∓ amount | Total-duration-preserving boundary move — congruent concept exists. |
| **SLIP SRC / SLIP DEST** | slip (Alt+drag) / move (drag body) with length fixed; or nudge left+right edges together | Vocabulary is *already* REAPER's (S/D editing). |
| **RIPL DEL** | "Items: Remove items" + ripple (Alt+click default) | Congruent. |
| **ROLL double-tap (SLIDE)** | move item without changing neighbors' content (ripple on = same as drag) | Merge into MOVE/SLIP pair in the mapping. |
| **ESC double-tap (UNDO)** | `Ctrl+Z` | Congruent. |
| **AUDIO LEVEL hold+wheel** | nudge item fade-in/out, or item volume via `GetSetMediaItemTakeInfo_Value` | Plausible; task 3. |

Loop: `Tab` to transient → `S` → `Ctrl+,`/edge nudge to tighten →
`Ctrl+Z` when wrong. Everything is available by keyboard already; the
device replaces the *hand on the keyboard*, not the concepts.

---

## 6. Workflow: transitions

REAPER has **no transition objects** — transitions are **crossfades at item
edges** (and envelope fades):

- **Auto-crossfade**: when items overlap, REAPER creates a crossfade
  automatically; `Alt+X` toggles the preference *(verify in app)*.
- **Crossfade within time selection**: `X` — the "smooth cut" workhorse:
  select the jump-cut region → crossfade everything inside.
- Crossfade lengths are adjustable per item (crossfade editor,
  `CrossfadeEditor_*` API family, API subset index).

Mapping: **TRANS** / **DIS** / **SMTH CUT** all converge on
crossfade-at-cursor / crossfade-within-selection; **TRANS DUR**
(hold+wheel) = adjust crossfade length at cursor. The device keeps all its
labels; REAPER's single concept (crossfade) absorbs three Resolve
concepts. **CUT** (remove transition) = remove crossfade at cursor.

---

## 7. Workflow: markers, takes, view

- **Markers**: `M` insert at cursor, `Shift+M` insert/edit, `[` / `]`
  previous/next marker, `Ctrl+1…0` move named marker to cursor. Device:
  **AUDIO LEVEL double-tap (MARK)** maps 1:1.
- **Takes**: every item can hold multiple takes (comping). `T` / `Shift+T`
  = next/previous take (all selected items), `Y` = activate take under
  mouse, take lanes (F2 item properties) to show them. This is the
  multicam substitute, §8.
- **View & navigation**: `Ctrl+wheel` = horizontal zoom (API:
  `adjustZoom` / `CSurf_OnZoom`); "View: Zoom to project" /
  "Zoom to time selection"; **follow edit cursor** toggle (keeps the
  arrange scrolled under the playhead while it moves — essential for
  wheel-shuttle use); SCRL wheel → `CSurf_OnScroll`.
- **Undo/Redo**: `Ctrl+Z` / `Ctrl+Shift+Z`. REAPER's undo is granular and
  reliable — the UNDO-on-double-tap grammar carries over verbatim.

---

## 8. Multicam → takes & multitrack

Resolve's multicam (CAM 1–9 + SYNC BIN + live override) has no 1:1 REAPER
equivalent, but two layers cover most of the use case:

1. **Takes** (any REAPER version): take N on an item = "camera N" for that
   time. CAM 1–9 → "set active take N on selected items"
   (`I_CURTAKE` item info, `CountTakes`, `GetActiveTake` — API subset §8).
   Editors comp takes exactly like multicam cutting. LED-per-CAM →
   "take N active on selection".
2. **Multitrack** (REAPER 7+): a project feature that syncs multiple takes
   across multiple items (timecode/beat alignment) and lets you switch the
   active take across a whole clip — the closest thing to a synced
   multicam bin *(verify exact feature name/status in app)*. SYNC BIN →
   "create/enable multitrack sync from selected items" (task 3 scope).
3. **Live override** (hold CAM + jog to write switches) has no native
   REAPER analog; it is a plugin behavior (jog + held take button → write
   active-take change as you pass item boundaries) — a later milestone.

---

## 9. Hidden-state inventory (what the LEDs must show)

State that silently changes what a button does, REAPER-side:

| state | values | how it changes behavior | LED candidate |
|---|---|---|---|
| **Ripple mode** | off / per-track / all-tracks | trims/deletes/moves close gaps or leave gaps | SNAP-adjacent; needs 3-value encoding |
| **Trim content behind items** | on/off | tape-style content slide vs. hidden trims | share ripple LED group |
| **Edit mode** | default / item / ripple / trim / nudge / MIDI | redefines all mouse gestures (and some actions) | one of the 18 panel LEDs |
| **Magnet/snap** | on/off | nudge & move quantization | **SNAP** button LED (1:1) |
| **Repeat (loop)** | on/off | wheel-stop semantics, "play section again" | STOP/PLAY cluster |
| **Follow edit cursor** | on/off | whether SCRL/SHTL view tracks the playhead | — |
| **Auto-crossfade** | on/off | whether overlapping inserts make transitions | TRANS LED family |
| **Time selection** | present/absent | IN/OUT-equivalents, crossfade region, nudge unit | IN/OUT are unlit; status LED |
| **Multitrack / active take** | none / take 1–N | what CAM buttons change | CAM 1–9 LEDs (already exist) |
| **Wheel mode** | JOG / SHTL / SCRL | the wheel's entire meaning | the 3 jog LEDs (1:1) |

---

## 10. Congruence summary: device label → REAPER concept

| label | congruent in REAPER? | concept |
|---|---|---|
| STOP/PLAY, SHTL, JOG, SCRL | ✅ | transport + (our own) shuttle law via playrate/scrub API |
| SPLIT, ROLL, SLIP SRC/DEST, RIPL DEL, APPND, IN/OUT (as trim points/time sel) | ✅ | native actions, §3–5 |
| SMART INSRT | ✅ (with design choice) | cursor insert w/ project ripple, or nearest-edge insert (plugin-implemented) |
| TRIM IN/OUT, ROLL-hold MOVE | ✅ | trim-to-cursor / nudge / move |
| TRANS, DIS, SMTH CUT, TRANS DUR, CUT | ✅ (converging) | crossfades, §6 |
| AUDIO LEVEL (+MARK 2nd label), SNAP, ESC (UNDO) | ✅ | item vol/fades, marker, magnet, undo |
| SOURCE / TIMELINE | ⚠️ reinterpret | no source viewer; SOURCE ≈ Media Explorer focus / source-parameters, TIMELINE ≈ arrange focus |
| CAM 1–9, SYNC BIN, LIVE O/WR | ⚠️ partial | takes + (7+) multitrack, §8; live override is plugin behavior |
| VIDEO_ONLY / AUDIO_ONLY | ⚠️ weak | no video-cut-page analog; closest = mute/solo audio track groups or show/hide — task 3 |
| FULL VIEW | ⚠️ weak | maximize arrange; no "review" concept without video |
| CLOSE UP (punch-in track) | ⚠️ composite | no native analog; plugin composite (new track + zoomed copy) |
| PLACE ON TOP, RIPL O/WR, SRC O/WR | ⚠️ composite | implement as multi-step action sequences |

**Bottom line for task 3:** REAPER natively covers the transport,
refinement, crossfade, marker, and (via takes) most of the multicam
labels with 1:1 vocabulary; the Cut-page *assembly* cluster (SOURCE,
Source Tape, Smart Indicator, punch-in, overwrite variants) is where the
plugin either reinterprets the labels toward REAPER's idiom (Media
Explorer, time selection, composite actions) or implements the missing
targeting (nearest-edge) itself. Ripple's 3-state nature and edit modes
must surface on LEDs or the device becomes a footgun.

## Sources

- Tutorial Tactic — *203 Reaper Keyboard Shortcuts + Cheat Sheet* (default
  keymap verified: split/ripple/trim-behind/crossfade/nudge/transient/
  marker/take/snap shortcuts): <https://tutorialtactic.com/blog/reaper-shortcuts/>
- Uros Baric — *Editing Classical Music in Reaper* parts 1–3 (S/D editing,
  split-all-tracks behavior, ripple per-track vs all-tracks, tape-style
  workflow, 3/4-point S/D editing via community actions):
  <https://urosbaric.com/reaper-classical-music-editing>,
  <https://urosbaric.com/reaper-classical-music-editing-2>,
  <https://urosbaric.com/reaper-classical-music-editing-3>
- Airwiggles — *REAPER Ripple Edit Change in 7.35* (toolbar ripple button
  behavior change; "Options: Cycle ripple editing mode" = command 1155):
  <https://www.airwiggles.com/c/general/reaper-ripple-edit-change-in-7-35>
- Cockos forums / community — shuttle setups for REAPER (no built-in
  analog shuttle; ShuttlePRO + playrate actions):
  <https://www.reddit.com/r/podcasting/comments/v7siew/do_you_edit_in_reaper_do_you_use_a_jogshuttle_how/>
- Repo: `docs/reaper-api-subset.md` (§5 CSurf transport/scrub/scroll/zoom,
  §6 mouse modifiers & view, §8 item/take info incl. ripple insert flag),
  `docs/speed-editor-workflow.md` (device side),
  `docs/speed-editor-capabilities.md` (keycode/LED inventory).
