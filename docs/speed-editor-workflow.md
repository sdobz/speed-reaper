# Blackmagic Speed Editor — How an Editor Actually Uses It (DaVinci Resolve Workflow)

`speed-editor-capabilities.md` documents *what the hardware can do* (protocol,
keycodes, LEDs). This document documents *how a person operates the device*
with DaVinci Resolve — the workflows the buttons implement, the input grammar
(tap / double-tap / hold+rotate), and the app-side state the workflow depends
on. It is the reference for designing our REAPER plugin's mapping (task 3) so
the labeled terms on the device stay congruent to the actions they trigger.

Sources: Blackmagic Design's product documentation, DaVinci Resolve's Cut/Edit
page behavior as documented by hands-on reviews (linked at the bottom), and
the vendored reference implementations in `docs/speed-editor-examples/`
(second-label annotations in `bmd.py` / `key_map.h` corroborate every
double-function listed here).

---

## 1. The big picture: a Cut-page device

The Speed Editor is **designed around Resolve's Cut page** — the
fast-assembly editing page with one main track, a "Source Tape" of all media,
and a **Smart Indicator** arrow on the timeline ruler. It also works on the
Edit page. Any REAPER mapping that wants the device to "feel right" must model
these three Cut-page concepts, not just the buttons:

| Cut-page concept | What it is | Why the workflow depends on it |
|---|---|---|
| **Smart Indicator** | Animated white arrow on the timeline ruler at the *nearest edit point* to the playhead | Determines **which clip an edit button acts on**. With the playhead inside a clip, "Trim IN" may trim the *next* clip's head, and "Smart Insert" inserts *after* that clip, not at the playhead. Editors learn to read it constantly; it has quirks. |
| **Source Tape** | All clips in the media pool strung into one long stream in the source viewer | The assembly workflow is: tape → jog → IN/OUT → insert into timeline → repeat. No per-clip clicking. |
| **Ripple state** | Cut-page setting (on by default) making trims/deletes close gaps | **If Ripple is accidentally off, the same button produces a different result** (gap instead of ripple). Editors must learn to check it. Hidden state = workflow footgun; any port must surface it (LED!). |

### Focus model

Two buttons switch what the wheel and edit buttons act on — this is the
device's own "mode" concept:

- **SOURCE** — focus the source viewer / Source Tape. Wheel jogs tape position;
  IN/OUT mark on the tape; insert buttons push tape into the timeline.
  Pressing SOURCE again while IN/OUT are set **zooms the viewer into the
  marked region** for fine adjustment; ESC returns to the full tape.
- **TIMELINE** — focus the timeline. Wheel jogs/shuttles the timeline
  playhead; all edit/trim/transition buttons operate on the timeline.

Editors live in TIMELINE focus most of the time and hop to SOURCE to grab
clips. STOP/PLAY transport and ESC work in either focus.

---

## 2. The wheel: three modes, one workflow

The SHTL / JOG / SCRL buttons (each with its own LED) set the wheel's mode.
The wheel has **no physical clutch** — the mode button is the entire feel
control. Protocol mapping (from the vendored demo `speed-editor-demo.py` and
`bmd.py`):

| Mode button | Protocol mode | Editor's use |
|---|---|---|
| **JOG** | relative | **The default home.** Free, precise playhead movement; gentle turns ≈ frame-by-frame. Fine-tuning edits, hunting a frame. Most of the session is spent here. |
| **SCRL** | relative | **Fast navigation.** A spin zips the playhead across the project to find a section, then tap JOG to land precisely. Complements JOG: SCRL to cross the timeline, JOG to settle inside a shot. |
| **SHTL** | absolute (dead-zero) | **Shuttle:** variable-rate play, speed scales with wheel speed/direction, both directions. Like physical wind. Editors split on it — some rely on it, some prefer keyboard JKL scrubbing; it trades precision for speed. |

Workflow pattern: `SCRL` to travel → `JOG` to land → `SHTL` to preview a
section with motion → STOP/PLAY to actually play.

In JOG mode the wheel's delta is *since last report* (relative); in SHTL the
vendored demo selects `ABSOLUTE_DEADZERO` (position around a baseline with a
dead band — the wheel "rests" centered). A REAPER plugin can exploit the same
choice: relative for nudges/scrub, absolute with deadzone for
shuttle-speed control (center = stopped, distance from center = speed, sign =
direction).

---

## 3. The input grammar: tap / double-tap / hold+rotate

This is the single most important workflow fact for our plugin: **most
buttons have up to three behaviors**, and the secondary one is printed as a
second label on the key. The vendored key tables carry exactly these labels.
Resolve implements all of this in the **application** — the hardware sends
only "key held down / released", with **no auto-repeat and no hardware
double-press detection** (capabilities §3). So tap-counting and
hold+rotate are software behavior we would have to reimplement:

| Verb | Meaning in Resolve |
|---|---|
| single tap | primary function (top label) |
| double tap | secondary function (2nd label) |
| hold + rotate wheel | analog adjustment ("dial" function) |

Complete inventory of non-primary behaviors (2nd label = annotation from
`bmd.py`/`key_map.h`):

| Button | 2nd label | Double-tap / hold behavior (Resolve) |
|---|---|---|
| ESC | UNDO | double-tap = **Undo** (the undo button of the device) |
| IN / OUT | CLR | double-tap = **clear** the in / out point |
| SNAP | (=) | double-tap-hold + wheel = **zoom viewer size** |
| SPLIT | MOVE | hold + wheel = **MOVE** the selected clip along the timeline (reorder) |
| ROLL | SLIDE | double-tap = **Slide** (move clip, neighbors ripple; in/out fixed) |
| CLOSE UP | YPOS | hold + wheel = **Y-position** (reframe) the punch-in track |
| TRANS | TITLE | double-tap + wheel = cycle **title font** |
| TRANS DUR | SET | hold + wheel = set **transition duration**; double-tap = **SET current duration as default** |
| AUDIO LEVEL | MARK | double-tap = **place marker** at playhead; hold + wheel = **marker color** (double-press-hold) / **volume of selected clip** |
| FULL VIEW | RVW | review-view toggle (2nd label = fullscreen review) |

---

## 4. Workflow: assembly (source → timeline)

The core editing loop, all hands-on-device:

1. **SOURCE** → focus the Source Tape.
2. **SCRL/JOG** to travel; **SHTL** to preview.
3. **IN**, then **OUT** — mark the segment (double-tap IN/OUT to clear later).
4. Pick the insert verb (timeline focus, or the Cut page auto-targets it):

| Button | Insert operation (Resolve semantics) |
|---|---|
| **SMART INSRT** | Insert at the edit point **nearest the playhead** (Smart Indicator), push everything after it. The workhorse. |
| **APPND** | Append to the **end** of the timeline. Building a sequence in order. |
| **RIPL O/WR** | Ripple-overwrite: replace the clip at the target, ripple to the clip's new length. |
| **SRC O/WR** | Source overwrite: place the marked segment at the **matching timecode** position (advanced). |
| **PLACE ON TOP** | Drop a copy on a **new track above** the playhead — cutaways/B-roll without disturbing the main track. |

5. Rinse and repeat; the Cut page's **Ripple must stay ON** or trims/deletes
   below stop closing gaps (workflow footgun, §1).

---

## 5. Workflow: refinement (timeline edits)

All operate on the timeline focus, all act on **what the Smart Indicator
points at**, not blindly on the playhead:

| Button | Operation | When an editor reaches for it |
|---|---|---|
| **SPLIT** | Cut clip at playhead (razor) | Most-used with TRIM IN/OUT |
| **TRIM IN / TRIM OUT** | Trim the clip edge at/near the indicator | Fine trims; **watch the indicator** or you trim the wrong clip |
| **ROLL** | Roll the **edit point**: move both edges of the two meeting clips, total duration unchanged | Fixing the beat of a cut between two clips |
| **ROLL double-tap (SLIDE)** | Slide: move a whole clip, neighbors ripple, in/out fixed | Repositioning a clip without re-trimming |
| **SLIP SRC / SLIP DEST** | Slip: change what the clip *shows* (source position) or where it *sits* (destination), duration & position fixed respectively | Fixing the content of a locked-length clip |
| **RIPL DEL** | Ripple delete: remove clip, close gap | Cutting dead material |
| **SPLIT hold+wheel (MOVE)** | Drag the selected clip to a new position | Reordering the story |
| **CLOSE UP** | Create a **punch-in track** (2× zoom of current clip on a new track above) | Instant cut-in; then YPOS-hold+wheel to reframe |

Typical refinement loop: SPLIT where the cut should be → TRIM IN/OUT to
tighten → ROLL to move the meeting point → (SPLIT-hold) MOVE to reorder →
ESC-double-tap (UNDO) whenever it goes wrong.

---

## 6. Workflow: transitions

| Button | Operation |
|---|---|
| **TRANS** | Insert a transition (default cross-dissolve) at the playhead. **Hold + wheel = scroll transition types.** |
| **CUT** | In single-cam mode: **remove** the transition at the playhead. |
| **DIS** | Add a dissolve at the playhead. |
| **SMTH CUT** | Smooth Cut — crossfade across a jump cut (classic: removing "umms and ahs" in interviews). |
| **TRANS DUR** | Hold + wheel = set transition duration; double-tap = save current as the default duration. |

Workflow: place playhead on the edit → TRANS → hold TRANS DUR + fine-tune
with wheel → (double-tap TRANS DUR to make it the new default).

---

## 7. Workflow: markers, audio, view

- **AUDIO LEVEL double-tap** = marker at playhead; hold + wheel cycles marker
  color. (Timeline annotation without touching the mouse.)
- **AUDIO LEVEL hold + wheel** = volume of the selected clip (quick mix fix
  during assembly).
- **SNAP** (LED) = magnet/snapping toggle — **state that should be visible**;
  the LED is the device's status bar for it.
- **VIDEO_ONLY / AUDIO_ONLY** (LEDs) = view filters on the Cut page.
- **FULL VIEW** = fullscreen review of the program.
- **STOP/PLAY** = the spacebar; thumb-positioned, the transport anchor of the
  whole workflow.
- **ESC** = escape the current focus / zoomed view (e.g. back from a zoomed
  IN/OUT region to the full Source Tape).

---

## 8. Workflow: multicam

- **SYNC BIN** = build/sync a multicam bin from the media pool.
- **CAM 1–9** = select the active camera for the multicam clip at the
  playhead; **LEDs mirror the active camera** (one of the 18 main-panel LEDs).
- **Live override (LIVE O/WR / "RND"):** hold a CAM button while **jogging**
  in JOG mode → the selected camera is written to the timeline **in real time
  as you advance**; LIVE O/WR locks/arms that mode. This is the signature
  multicam workflow: jog through the take while painting camera switches by
  holding buttons.

---

## 9. LED & battery semantics (what the device tells you)

- **JOG/SHTL/SCRL LEDs** = which wheel mode is active (normally exactly one lit).
- **Main-panel LEDs** = persistent state the host owns: SNAP on, active CAM,
  VIDEO_ONLY/AUDIO_ONLY filter, LIVE O/WR armed, plus per-button "on" states
  (e.g. CLOSE UP punch-in track present).
- **Battery**: no on-device indicator; Resolve reads it (HID report `0x07`)
  and surfaces it under *Preferences → Control Panels*. The plugin can read
  the same report directly (charging flag + 0–100 level).
- 22 of the 43 buttons are unlit by design (all trim/point/insert buttons) —
  the lit set is a **status bar**, not a per-key backlit map.

---

## 10. Implications for a REAPER port (what "same workflow" requires)

Facts that constrain any faithful port (design decisions belong to
`reaper-plugin-workflow.md`, task 3):

1. **Double-tap and hold+rotate are app behavior.** The hardware only
   reports held keysets (report `0x04`, no auto-repeat, no repeat, no
   double-press detection). The plugin needs a small per-key gesture engine
   (tap debounce, hold detection, "held key + wheel delta = adjust").
2. **Focus model (SOURCE vs TIMELINE) is the wheel's target selector.** In
   REAPER terms this is "what does the wheel act on" — e.g. playhead on
   arrange vs. media-explorer/take position; the wheel-mode buttons (JOG/
   SHTL/SCRL) and the focus buttons are *two independent axes*.
3. **A Smart-Indicator equivalent changes which item every edit button hits.**
   REAPER's edit cursor already has "nearest item" semantics in many actions;
   the mapping must pick actions that target the *closest edit boundary*, or
   the buttons will feel wrong.
4. **Ripple is hidden state.** Resolve's Cut page defaults it ON; the port
   should track and display it (SNAP-style LED is the obvious home) or the
   same button silently changes behavior.
5. **Absolute-deadzero mode (SHTL) is the natural shuttle control law**:
   wheel centered = stopped, offset = speed, sign = direction — vs. relative
   deltas for jog/scroll.
6. **LEDs are the status surface**: mode + snap/ripple + active selection +
   transport. Only 21 buttons can light; choose state, not cosmetics.
7. **Device exclusivity + re-auth** (capabilities §1) are session-level
   workflow facts: the device must not be claimed by Resolve while the
   plugin runs, and auth must re-run periodically (~600 s timeout observed).

## Sources

- Capture Guide — *A Beginner's Guide to Using the DaVinci Resolve Speed
  Editor* (hands-on workflow: dial modes, Smart Indicator, double-tap /
  hold+rotate inventory, insert/trim/transition/multicam workflows, ripple
  footgun): <https://www.captureguide.com/davinci-resolve-speed-editor-guide/>
- Blackmagic Design — Speed Editor product page (jog/shuttle/scroll
  description, thumb-stop transport, camera/sync-bin multicam + live
  override): <https://www.blackmagicdesign.com/products/davinciresolve/keyboard>
- ProVideo Coalition — *Review: DaVinci Resolve Speed Editor, Part 1*
  (Cut-page emphasis; no clutch on the dial; SOURCE/TIMELINE/ESC as daily
  drivers): <https://www.provideocoalition.com/review-davinci-resolve-speed-editor-part-1/>
- Mixing Light — *The DaVinci Resolve Speed Editor on the Color, Edit, and
  Cut Pages* (scroll-mode usage, double-press secondary labels):
  <https://mixinglight.com/color-grading-tutorials/the-davinci-resolve-speed-editor-on-the-color-edit-cut-pages/>
- Vendored references: `docs/speed-editor-examples/python-bmd/bmd.py`
  (second-label annotations), `python-bmd/speed-editor-demo.py` (jog-mode
  choices per button), `cpp-driver/src/key_map.h` (keycode table) —
  protocol facts in `docs/speed-editor-capabilities.md`.
