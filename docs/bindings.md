# Speed Reaper bindings reference

This file is **generated** — do not edit it. `tools/check` regenerates it
and fails if the committed copy differs from the registry.

## Where the bindings file lives

`<REAPER resource path>/speed-reaper/bindings.ini`. If it is missing, the
first plugin run creates it seeded from the default file at the bottom of
this document (it is never overwritten afterwards); an unreadable file
falls back to the same default in memory.

- Linux: `~/.config/REAPER/speed-reaper/bindings.ini`
- Windows: `%APPDATA%\REAPER\speed-reaper\bindings.ini`

## Syntax

```ini
[tuning]
double_tap_ms = 300        ; double-tap window (default 300)
hold_ms = 350              ; hold detection (default 350)
hold_ticks_per_step = 4    ; wheel ticks per turn-verb step (default 4)

[bindings]
KEY.gesture = verb [args]  ; comments start with ;
```

- `KEY` is a key name from the table below — the `KEY_*` suffix from
  `src/speed_editor/key_map.h`, i.e. the printed label with spaces and
  slashes replaced (`STOP/PLAY` reads `STOP_PLAY`, `RIPL O/WR` reads
  `RIPL_OWR`).
- `gesture` is one of:
  - `press` — hold and release without rotating;
  - `tap` — a quick press;
  - `double` — two taps within `double_tap_ms`;
  - `turn` — hold the key and rotate the wheel (one hold = one undo step).
  A key bound to `press` takes no other gesture.
- `verb [args]` must be a registered verb (table below) that accepts the
  gesture. Bad lines are skipped and reported as
  `bindings.ini:<line>: <reason>` — they never stop parsing.
- Turn verbs take an optional **`step=frame|grid|sample|<N>ms`** argument:
  the time per step (default `frame` = one project frame, applied every
  `hold_ticks_per_step` wheel ticks).
- After editing, run **Speed Reaper: reload bindings** to apply.

## Plugin actions (action list: search “Speed Reaper”)

| action | does |
| --- | --- |
| `Speed Reaper: reload bindings` | re-reads and re-parses the file |
| `Speed Reaper: open bindings file` | opens it in the default editor |
| `Speed Reaper: show bindings` | prints the current bindings (and unbound keys) to the console |
| `Speed Reaper: echo fired bindings (toggle)` | additionally messages every fired binding |
| `Speed Reaper: learn (pick an action, then press a device key)` | binds the pressed key+gesture to the picked action, writing it to the file and reloading |

## Keys

| key | label | keycode |
| --- | --- | --- |
| SMART_INSRT | SMART INSRT | 0x0001 |
| APPND | APPND | 0x0002 |
| RIPL_OWR | RIPL O/WR | 0x0003 |
| CLOSE_UP | CLOSE UP | 0x0004 |
| PLACE_ON_TOP | PLACE ON TOP | 0x0005 |
| SRC_OWR | SRC O/WR | 0x0006 |
| IN | IN | 0x0007 |
| OUT | OUT | 0x0008 |
| TRIM_IN | TRIM IN | 0x0009 |
| TRIM_OUT | TRIM OUT | 0x000A |
| ROLL | ROLL | 0x000B |
| SLIP_SRC | SLIP SRC | 0x000C |
| SLIP_DEST | SLIP DEST | 0x000D |
| TRANS_DUR | TRANS DUR | 0x000E |
| CUT | CUT | 0x000F |
| DIS | DIS | 0x0010 |
| SMTH_CUT | SMTH CUT | 0x0011 |
| SOURCE | SOURCE | 0x001A |
| TIMELINE | TIMELINE | 0x001B |
| SHTL | SHTL | 0x001C |
| JOG | JOG | 0x001D |
| SCRL | SCRL | 0x001E |
| SYNC_BIN | SYNC BIN | 0x001F |
| TRANS | TRANS | 0x0022 |
| VIDEO_ONLY | VIDEO ONLY | 0x0025 |
| AUDIO_ONLY | AUDIO ONLY | 0x0026 |
| RIPL_DEL | RIPL DEL | 0x002B |
| AUDIO_LEVEL | AUDIO LEVEL | 0x002C |
| FULL_VIEW | FULL VIEW | 0x002D |
| SNAP | SNAP | 0x002E |
| SPLIT | SPLIT | 0x002F |
| LIVE_OWR | LIVE O/WR | 0x0030 |
| ESC | ESC | 0x0031 |
| CAM1 | CAM 1 | 0x0033 |
| CAM2 | CAM 2 | 0x0034 |
| CAM3 | CAM 3 | 0x0035 |
| CAM4 | CAM 4 | 0x0036 |
| CAM5 | CAM 5 | 0x0037 |
| CAM6 | CAM 6 | 0x0038 |
| CAM7 | CAM 7 | 0x0039 |
| CAM8 | CAM 8 | 0x003A |
| CAM9 | CAM 9 | 0x003B |
| STOP_PLAY | STOP/PLAY | 0x003C |

## Verbs

Generated from `MakeDefaultRegistry()` (plugin/src/verb_registry.cpp).
`args` is the argument syntax; verbs with `—` take no arguments.

| verb | gestures | args | description |
| --- | --- | --- | --- |
| action | press, tap, double | `<id>` | Run REAPER command <id> directly |
| isolate_tracks | tap | `—` | Hide every track except the targets' tracks (tap again to restore) |
| nudge | turn | `<move\|trim_in\|trim_out\|slip_src\|slip_dest\|volume\|fade_in\|fade_out\|roll\|sel_in\|sel_out> [step=frame\|grid\|sample\|<N>ms]` | Hold+rotate nudge of the targets: <move\|trim_in\|trim_out\|slip_src\|slip_dest\|volume\|fade_in\|fade_out\|roll\|sel_in\|sel_out> [step=frame\|grid\|sample\|<N>ms] |
| place_on_top | tap | `—` | Duplicate the targets onto a new track above each source track |
| ripple_delete | tap, double | `—` | Remove the targets (or time selection) and close the gap |
| select_cursor_item | tap | `—` | Select the item under the cursor exclusively (tap again to deselect) |
| select_step | turn | `—` | Step the selection to the next/previous item on the target track (20 ticks per step) |
| smooth_cut | tap | `—` | Smooth-cut at the time selection |
| solo_tracks | tap | `—` | Solo the targets' tracks (tap again to restore) |
| take | tap, double | `<N>` | Set the active take <N> on the targets |
| targeted | press, tap, double | `<id>` | Run REAPER command <id> on the implicit targets, one undo step |
| transport | press | `<play_stop>` | Transport <play_stop> (a SHTL play_stop is a hard stop back to the prior mode) |
| volume_readout | tap | `—` | Read out the first target item's volume in dB |
| wheel | press | `<jog\|scrl\|shtl>` | Switch the wheel mode: <jog\|scrl\|shtl> |

## Default bindings file

`plugin/src/default_bindings.ini` — what first run seeds the file with
(verbatim):

```ini
; Default Speed Reaper bindings (41-action-registry §3).
; Reproduces exactly today's behavior: the old id + composite rows /
; the old hold rows tables in plugin/src/verbs.cpp and the press rules in
; plugin/src/router.cpp — one commented line per row, comment = what it
; does in REAPER terms. Shipped as the first-run bindings file; parsed by
; ParseBindings (plugin/src/bindings.cpp), embedded at build time.

[tuning]
; Defaults from GestureConfig (gesture.h); hold_ticks_per_step is the
; turn-verb (nudge) step granularity from the old hold rows.
double_tap_ms = 300
hold_ms = 350
hold_ticks_per_step = 4

[bindings]
; --- Router press rules (router.cpp OnButton) -------------------------
JOG.press       = wheel jog        ; wheel mode: jog the playhead with the wheel
SCRL.press      = wheel scrl       ; wheel mode: scroll the arrange view
SHTL.press      = wheel shtl       ; wheel mode: shuttle (absolute transport rate)
STOP_PLAY.press = transport play_stop ; toggle play/stop (40044); in SHTL: hard stop, back to the prior mode

; --- the old id table: (key, gesture) → REAPER command id --------------------
SPLIT.tap         = action 40012   ; Item: Split items at edit or play cursor
IN.tap            = action 40625   ; Time selection: Set start point
OUT.tap           = action 40626   ; Time selection: Set end point
IN.double         = action 40635   ; Time selection: Remove (unselect) time selection
OUT.double        = action 40635   ; Time selection: Remove (unselect) time selection
SNAP.tap          = action 1157    ; Options: Toggle snapping
ESC.tap           = action 40635   ; Time selection: Remove (unselect) time selection
ESC.double        = action 40029   ; Edit: Undo
AUDIO_LEVEL.double = action 40157  ; Markers: Insert marker at current position
FULL_VIEW.tap     = action 40346   ; Toggle fullscreen

; --- the old composite rows: function-verb rows (win over the id table) ---------
TRIM_IN.tap       = targeted 40511 ; trim the target's left edge to the cursor, one undo step
TRIM_OUT.tap      = targeted 40512 ; trim the target's right edge to the cursor, one undo step
RIPL_DEL.tap      = ripple_delete  ; remove the time-sel contents, or the target items, closing the gap
TIMELINE.tap      = select_cursor_item ; select the item under the cursor exclusively (tap again: deselect)
TIMELINE.double   = action 40289   ; deselect all items
CAM1.tap          = take 1         ; set active take 1 on the target items
CAM2.tap          = take 2         ; set active take 2 on the target items
CAM3.tap          = take 3         ; set active take 3 on the target items
CAM4.tap          = take 4         ; set active take 4 on the target items
CAM5.tap          = take 5         ; set active take 5 on the target items
CAM6.tap          = take 6         ; set active take 6 on the target items
CAM7.tap          = take 7         ; set active take 7 on the target items
CAM8.tap          = take 8         ; set active take 8 on the target items
CAM9.tap          = take 9         ; set active take 9 on the target items
AUDIO_LEVEL.tap   = volume_readout ; read out the first target's volume as "Volume: x.x dB"
SMTH_CUT.tap      = smooth_cut     ; smooth-cut (40916) — only with a time selection
PLACE_ON_TOP.tap  = place_on_top   ; duplicate the targets onto one new track above
VIDEO_ONLY.tap    = isolate_tracks ; hide every track except the targets' tracks (toggle)
AUDIO_ONLY.tap    = solo_tracks    ; solo the targets' tracks (toggle)

; --- the old hold rows: hold + wheel (one undo block per hold) -----------------
SPLIT.turn        = nudge move         ; move the target items, one frame per 4 wheel ticks
TRIM_IN.turn      = nudge trim_in      ; drag the target's left edge, one frame per 4 ticks
TRIM_OUT.turn     = nudge trim_out     ; drag the target's right edge, one frame per 4 ticks
SLIP_SRC.turn     = nudge slip_src     ; slip the target's source edge, one frame per 4 ticks
SLIP_DEST.turn    = nudge slip_dest    ; slip the target's destination edge, one frame per 4 ticks
AUDIO_LEVEL.turn  = nudge volume       ; change the target's volume, one frame step per 4 ticks

; --- the last CompositeRow: TIMELINE hold+rotate ------------------------
TIMELINE.turn     = select_step        ; step the selection to the next/previous item (20 ticks per step)

; --- more turn handles (hold the key + turn the wheel) -------------------
IN.turn           = nudge sel_in       ; move the time selection start, one frame per 4 ticks
OUT.turn          = nudge sel_out      ; move the time selection end, one frame per 4 ticks
ROLL.turn         = nudge roll         ; roll the edit point between the cursor item and its right neighbour
; Suggestions (uncomment to use; step=frame|grid|sample|<N>ms on any nudge):
; TRANS_DUR.turn  = nudge fade_in step=10ms   ; drag the target's fade-in length
; DIS.turn        = nudge fade_out step=10ms  ; drag the target's fade-out length
; SPLIT.turn      = nudge move step=grid      ; move the targets on the grid

; --- crossfades (docs/notes/crossfade-actions.md) ------------------------
TRANS.tap         = action 41059       ; Item: Crossfade any overlapping items (selected items)
DIS.tap           = action 40916       ; Item: Crossfade items within time selection
CUT.tap           = action 41193       ; Item: Remove fade in and fade out (removes the crossfade)
TRANS_DUR.tap     = action 43776       ; Item: Set crossfade length to default

; --- shipped UNBOUND: no clear REAPER meaning for the printed label -------
; Uncomment a line (or run "Speed Reaper: learn") to bind it. Ids are plain
; REAPER actions from docs/reaper-actions.tsv.
; SMART_INSRT.tap = action 40142   ; Insert empty item
; SMART_INSRT.tap = action 40001   ; Track: Insert new track
; APPND.tap       = action 41295   ; Item: Duplicate items
; APPND.tap       = action 42432   ; Item: Glue items within time selection
; RIPL_OWR.tap    = action 1162    ; Toggle ripple editing on/off
; RIPL_OWR.tap    = action 41991   ; Toggle ripple editing all tracks
; SRC_OWR.tap     = action 40270   ; Item: Reverse items to new take
; SRC_OWR.tap     = action 40175   ; Item properties: Toggle mute
; CLOSE_UP.tap    = action 41622   ; View: Toggle zoom to selected items
; CLOSE_UP.tap    = action 40717   ; Item: Select all items in current time selection
; SYNC_BIN.tap    = action 40157   ; Markers: Insert marker at current position
; SYNC_BIN.tap    = action 1068    ; Transport: Toggle repeat
; LIVE_OWR.tap    = action 1013    ; Transport: Record
; LIVE_OWR.tap    = action 1068    ; Transport: Toggle repeat
; SOURCE.tap      = action 40182   ; Item: Select all items
; SOURCE.tap      = action 40717   ; Item: Select all items in current time selection
```
