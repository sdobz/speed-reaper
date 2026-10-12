# Plugin design for milestones M5–M9 (normative for `vibes/31…36`)

Follows `plugin-design-m1-m3.md` (§7 gestures + verbs stay as is). Behavior
source: `reaper-plugin-workflow.md` (§5 targeting, §6 LEDs, §7 mapping) and
`reaper-workflow.md`. Every command id below is a **candidate**: the oracle
scenario of the milestone proves it by behavior; a wrong id is a one-line
fix in the table, recorded in `docs/notes/m5-observations.md` (create on
first need).

## §1 The selection problem and the decision

The Speed Editor has **no select button**, yet most REAPER item actions
(`Item: Remove items`, trim, nudge, take switch) act on *selected items*.
Resolve solves this with the Smart Indicator; we do two things:

1. **Implicit target (`TargetItems`)** — every item verb works without any
   selection. Rule, in order:
   1. If any item is selected → the selected items (REAPER-native, unchanged).
   2. Else the item on the *target track* that contains the edit cursor
      (`pos <= cursor < pos+len`; at the very end of a track's last item,
      that item). Target track = first selected track, else last-touched
      track. At most one item.
   3. Else nothing: the verb does nothing and traces `target none`.
   The verb runs through `WithTargetSelected`: snapshot selection → select
   the targets (only in case 2) → run → restore the snapshot. Net selection
   change = none (workflow §5 "device never silently changes selection").
2. **Explicit select, on TIMELINE (the "re-assert" key)** — the TIMELINE tap
   was a no-op in workflow §1; it becomes the select key:
   - Tap = select the `TargetItems` of rule 2 exclusively (deselect others).
     Tap again with that item already the sole selection = deselect all.
   - Double-tap = deselect all items (`40289`).
   - Hold + rotate = move the selection to the next / previous item on the
     target track, one item per `kSelectStepTicks` (= 20) wheel ticks (the
     cursor does not move).
   When SOURCE focus lands (B3) a TIMELINE tap while in SOURCE focus
   switches focus *first* and does not select; in TIMELINE focus it
   selects. Until then there is no focus state.

## §2 Pure interfaces (REAPER-free, unit-tested)

`plugin/src/project.h`:

```cpp
struct ItemInfo { int id; int track; double pos; double len; bool selected; };
class IProject {
 public:
  virtual ~IProject() = default;
  virtual std::vector<ItemInfo> Items() = 0;       // all items, ordered by track then pos
  virtual std::vector<int> SelectedTracks() = 0;   // track indices
  virtual int LastTouchedTrack() = 0;              // -1 if none
  virtual double CursorPos() = 0;
  virtual bool HasTimeSelection() = 0;
  virtual void SetSelectedItems(const std::vector<int> &ids) = 0;
  virtual int RippleMode() = 0;                    // 0 off, 1 per-track, 2 all
  virtual void Undo_Begin() = 0;                   // Undo_BeginBlock2
  virtual void Undo_End(const char *label) = 0;    // Undo_EndBlock2 (one step)
};
```

`plugin/src/target.h/.cpp`: `std::vector<int> TargetItems(IProject&)` (§1.1)
and `template<class F> void WithTargetSelected(IProject&, F)`.
`ReaperProject` (in `main.cpp` or `reaper_project.cpp`, not pure) implements
`IProject` with `CountMediaItems`/`GetMediaItem`/`IsMediaItemSelected` etc.;
`id` = index in `Items()` order. A fake `IProject` lives in `plugin/test/`.

`VerbRouter` gains an `IProject&` and a second table of **composite verbs**
(function pointers `void(*)(IActionSink&, IProject&)`) next to the command-id
table; composite rows win over id rows for the same (key, kind).

## §3 Verb table additions

All rows: tap unless stated; "T" = runs through `WithTargetSelected` and one
undo block.

| key | gesture | behavior | id / API (candidate) |
|---|---|---|---|
| RIPL DEL | Tap | **time selection present** → remove its contents and close the gap, all tracks | `40201` |
| | | else T: remove target items **and close the gap**: if `RippleMode()==0`, switch to per-track (`40310`), remove (`40006`), restore (`40309`); else just `40006` | |
| TRIM IN / OUT | Tap | T: existing ids `40511` / `40512` | |
| TIMELINE | Tap / DoubleTap / HoldRotate | select verbs, §1.2 | `40289` for clear |
| CAM 1–9 | Tap | T: set active take N (`I_CURTAKE = N-1`, clamped to `CountTakes`) on each target item; items with fewer takes keep theirs | API |
| AUDIO LEVEL | DoubleTap | project marker at cursor | `40157` |
| AUDIO LEVEL | Tap | message box: target items' volume in dB (`D_VOL`) | API |
| SMTH CUT | Tap | crossfade within time selection (`X`); no time selection → trace `no time selection` and nothing else | `40916` |
| PLACE ON TOP | Tap | T: duplicate target items onto a **new track above** their track, same position | API |
| VIDEO_ONLY | Tap | toggle isolate view: hide all tracks except those of the target items; second tap restores the saved visibility | API (`B_SHOWINTCP`/`B_SHOWINMIXER`) |
| AUDIO_ONLY | Tap | toggle solo on the target items' tracks; second tap restores the saved solo flags | API (`I_SOLO`) |
| FULL VIEW | Tap | toggle maximized arrange | candidate `40346`; settle by oracle |

SPLIT stays `40012` (REAPER `S` semantics: nothing selected = all tracks; do
**not** wrap it). IN/OUT/SNAP/ESC unchanged.

## §4 Hold + rotate (M7)

`VerbRouter` handles `HoldStart` / `HoldRotate` / `HoldEnd`. One hold = one
undo block: `Undo_Begin` at `HoldStart`, `Undo_End` at `HoldEnd`. The target
set is resolved **once at `HoldStart`** (and selection restored at
`HoldEnd`). Step size: `step = frame (1/project fps, 1/30 s if unknown)`;
`kHoldTicksPerStep = 4` wheel ticks per step (accumulate remainder; sign =
direction). Row table:

| key (hold+rotate) | effect per step |
|---|---|
| SPLIT (MOVE) | move target items `pos += step` (`D_POSITION`) |
| TRIM IN | move the targets' left edge by `step`, content stays (`D_POSITION`, `D_LENGTH`, take `D_STARTOFFS` adjusted by `step * playrate`) |
| TRIM OUT | move the right edge by `step` (`D_LENGTH`) |
| SLIP SRC | `D_STARTOFFS += step * playrate`; position + length fixed |
| SLIP DEST | `D_POSITION += step` of the targets, **and** `D_STARTOFFS -= step * playrate` so content stays fixed in time |
| AUDIO LEVEL | `D_VOL` in 0.5 dB steps, clamped to 0 … +12 dB |

Edges are clamped: length stays ≥ one step, start offset ≥ 0. Edits are
**not** ripple-aware in v1 (document in smoke test).

## §5 LED status bar (M8)

`MainLedMask(const LedState&)` replaces `MainLedMask(bool)`:
`LedState { bool snap; int ripple /*0..2*/; bool trim_behind; int active_take /*0 none, 1..9*/; bool isolate; bool solo; }`.
Bits: SNAP, CUT = ripple==1, DIS = ripple==2, LIVE_OWR = trim_behind,
`CAM_n` for `active_take`, VIDEO_ONLY = isolate, AUDIO_ONLY = solo. Active
take = take number shared by **all** selected items (none selected or mixed
→ 0, dark). Reads (candidates, proven by oracle): snap `GetToggleCommandState(1157)`;
ripple `GetToggleCommandState(40310)` → per-track, `40311` → all; trim-behind
`GetToggleCommandState(41117)`; takes via API. Poll ≤ 250 ms (existing loop).
The device never writes these except through its own buttons.

## §6 Non-obvious buttons and the binding table (M9)

"Obvious" = label has a direct REAPER equivalent (§3–§5, plus crossfade in
M9's spike). **Non-obvious** = no single REAPER command: SMART INSRT, APPND,
RIPL O/WR, SRC O/WR, CLOSE UP (tap and hold), SYNC BIN, LIVE O/WR, TRANS /
DIS / CUT / TRANS DUR until the crossfade spike resolves, ROLL, SOURCE.
They ship **unassigned** and are made configurable, not guessed:

- A **binding table**, key = `<KEYNAME>.<gesture>` (e.g. `SYNC_BIN.tap`,
  `CLOSE_UP.hold`, `APPND.tap`), value = a REAPER command: numeric id or a
  named-command string (`_RSxxxx`, custom action ids). Stored via
  `SetExtState("SpeedEditor", key, value, true)`; loaded at startup into
  the same `VerbRouter` table; a binding **overrides** a built-in row only
  for the non-obvious set (the obvious set is not rebindable).
- Value `T:<id>` = run through `WithTargetSelected`; plain = run directly.
- Assign by hand: edit `reaper.ini` `[SpeedEditor]`, or the action
  `speed_reaper_bind` ("Speed Reaper: bind next pressed device key to the
  selected action"): `PromptForAction` flow of `plugin-configuration.md` §3.3.
- Config doc governs persistence, keys and defaults; no new decisions here.
