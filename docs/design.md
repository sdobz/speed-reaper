# Speed Reaper — design (as implemented)

The Blackmagic Speed Editor (HID joystick) as a REAPER controller: wheel
jogs / scrolls / shuttles the transport, buttons run REAPER actions and
item operations, and every key + gesture is user-bindable through a plain
text file. This doc describes the architecture as it is today; milestone
designs that preceded the current shape live in `docs/archive/`.

## 1. Data flow

```
HID worker thread                    REAPER main thread (timer callback)
─────────────────                    ────────────────────────────────────
HidWheelSource::Start                onTimer (every ~30 Hz)
  readEvent loop                       └─ Session::Tick(now_ms)
  accumulate wheel delta                  ├─ drain PopButton → GestureEngine::OnKey
  diff held keycodes                      ├─ GestureEngine::Poll → Pop(Gesture)
  (atomic int32 delta,                        └─ VerbRouter::OnGesture → bound Verb
   mutex + deque for buttons                 ├─ WheelDispatcher::Tick → mode routing
   and log lines)                              │    Jog  → JogEngine → IPlayheadSink
                                               │    Scrl → Router::OnWheel (view)
                                               │    Shtl → Router::OnShuttleOffset
                                               └─ LED poll every ~250 ms → RequestLeds
```

Only the REAPER timer callback may call REAPER APIs. The HID worker thread
never does; it produces an atomic wheel accumulator (relative modes), an
absolute position (SHTL mode), a mutex-protected button deque, and a log
deque that `main.cpp` drains into `ShowConsoleMsg`. Mode/LED requests flow
the other way: a mutex-protected "pending output" struct the worker applies
between reads.

All decision logic is **pure** (no `reaper_plugin.h`): `Session`,
`GestureEngine`, `Router`, `WheelDispatcher`, `JogEngine`, `ShuttleRate`,
`TargetItems`/`WithTargetSelected`, `ParseBindings`, `VerbRegistry`, the
verb implementations, and `MainLedMask`. `main.cpp` is the only TU with
REAPER glue: entry point, sinks (`ReaperActionSink`, `ReaperPlayheadSink`,
`ReaperProject` implementing `IProject`), the five actions, and the timer.
Unit tests (`plugin/test/*_test.cpp`) run the pure layer with fakes; the
headless oracle (`docs/plugin-oracle-design.md`) runs the real plugin in
headless REAPER with a fake device.

## 2. The device worker (`HidWheelSource`)

`plugin/src/hid_wheel_source.{h,cpp}` wraps the shared driver in
`src/speed_editor/` (`SpeedEditorHID`, `key_map.h` — protocol facts in
`docs/speed-editor-capabilities.md`):

1. Not open → retry `open()` every 1 s (sleeping in ≤100 ms slices so
   `Stop()` is prompt); log only on state change.
2. `authenticate()`; on failure log `auth failed`, close, back to 1.
3. `setWheelMode(0)` (relative), `setJogLeds(true,false,false)`, log
   `connected (serial …)`.
4. Loop: apply pending outputs (wheel mode, LED masks), then
   `readEvent(ev, 20)`. Relative wheel modes (0/2) accumulate
   `delta += ev.wheel.delta`; absolute modes (1/3) store the latest
   position. Button events: diff the reported keycode set against the
   previously held set (`DiffHeldKeys`: releases first, then presses) and
   push `ButtonChange{key, down}`.
5. `readEvent` false means timeout *or* disconnect: check
   `getState() == Disconnected` → log `disconnected`, clear the held set,
   back to 1.

Log strings are fixed (`connected (serial %s)`, `auth failed`,
`disconnected`, `device not found`); tests and the oracle grep them.
`plugin/tools/hid_probe.cpp` is the standalone exerciser (no REAPER).

## 3. `Session` — everything the timer callback does

`plugin/src/session.h`: one testable object owning the `GestureEngine`,
`Router`, `VerbRouter` (the bindings table), `WheelDispatcher`,
`JogEngine`, the LED mask state, the learn state, and the connect edge.
No clock, no REAPER API, no HID — all time arrives via `Tick(now_ms)`, the
device behind `IDevice`. One `Tick`:

- drain button events → `GestureEngine::OnKey`;
- `GestureEngine::Poll`, then each `Pop`ped gesture → the bound verb
  (or the learn capture, while learning);
- mode changes → `RequestWheelMode` + jog-mode LEDs;
- the false→true `Connected()` edge re-sends the current mode and LEDs;
- LED poll every ~250 ms (time-based), push the mask only on change;
- fixed-step engine catch-up: 8 ms steps, ≤50 ms of catch-up per call,
  backlog capped at 250 ms (so the playhead keeps moving, slightly late,
  under main-loop delay — never a large jump).

Also on `Session`: `ReloadBindings` (re-parse; console line
`bindings loaded n=<count>` plus one line per bad line), `ShowBindings`
(live table to the console), `ToggleEcho`, and `BeginLearn` (§11).

## 4. Gestures (`GestureEngine`)

`plugin/src/gesture.{h,cpp}` turns raw key down/up plus wheel motion into
four binding gestures (tuning in `[tuning]` of the bindings file):

- **press** — key-down, fires immediately, no tap wait (wheel modes,
  transport). A key bound with `press` must not have any other gesture.
- **tap** — released before `hold_ms` (default 350 ms). Keys *with a
  `.double` binding* defer their tap `double_tap_ms` (default 300 ms)
  after release; a second press in the window yields exactly one
  **double** (no tap). The double-tap set is derived from the bindings,
  never listed separately.
- **turn** — held ≥ `hold_ms` **or** wheel motion while held →
  `HoldStart` once, then `HoldRotate` per wheel delta, `HoldEnd` on
  release (no tap). `HoldActive()` tells the rest of the pipeline that a
  held key owns the wheel.

## 5. Bindings — the user-facing contract

File: `<GetResourcePath()>/speed-reaper/bindings.ini`. Written from a
compiled-in default on first run if missing; **never overwritten
afterwards** (the user's file is theirs). `plugin/src/bindings.cpp`
parses the text; `default_bindings.ini` (embedded at build time) is the
shipped manual — one commented line per default.

```ini
[tuning]
double_tap_ms = 300
hold_ms = 350
hold_ticks_per_step = 4

[bindings]
; KEY.gesture = verb [args]   ; comment
SPLIT.tap  = action 40012
SPLIT.turn = nudge move
JOG.press  = wheel jog
```

- **KEY** = the `KEY_*` suffix name from `src/speed_editor/key_map.h`
  (`SPLIT`, `TRIM_IN`, `STOP_PLAY`, `CAM1`…).
- **gesture** = `press | tap | double | turn` (§4).
- **verb** = a name from the registry, with args (§6).
- `;`/`#` start comments; unknown sections and bad lines never stop
  parsing — each yields one `bindings.ini:<n>: <reason>` error (console +
  trace) and is skipped. A file that cannot be read at all falls back to
  the embedded default.

**Why a text file.** The whole point of the device is that the user
redistributes 43 buttons across their own workflow; a bindings file is the
shape that supports that: it is human-editable in any editor, greppable,
diffable, and versionable without any REAPER UI; the shipped default *is*
the manual; the `learn` action writes one line into it programmatically
(`SetBindingLine` replaces in place, preserves every other byte); and
persistence needs no new mechanism — REAPER's resource directory survives
restarts and travels with the profile. (The earlier plan to store bindings
in REAPER `ExtState` was dropped: the file is inspectable outside REAPER
and needs no per-key ini namespace.)

## 6. Verbs (`VerbRegistry`)

`plugin/src/verb_registry.{h,cpp}`: named specs, each declaring the
gestures it accepts and a factory that builds a `Verb` from argument
strings. Binding errors (unknown name, gesture the verb does not accept,
bad args) are decided at parse time with a one-line message. A `Verb`
implements `Run` (one-shot gestures) and/or `Begin`/`Step`/`End` (turn
verbs). Registered verbs:

| verb | args | what it does |
|---|---|---|
| `action` | id or `_NamedCmd` | run a REAPER command (`Main_OnCommand` / `NamedCommandLookup`); unresolved → trace `bind unresolved`, no-op |
| `targeted` | id or `_NamedCmd` | `action` inside `WithTargetSelected` + one undo block |
| `wheel` | `jog \| scrl \| shtl` | set the wheel mode (shared `WheelState` in the registry) |
| `transport` | `play_stop` | toggle play/stop (40044); in SHTL: hard stop + return to prior mode |
| `nudge` | `move \| trim_in \| trim_out \| slip_src \| slip_dest \| volume \| sel_in \| sel_out \| roll \| fade_in \| fade_out`, `step=…` | turn handle, §8 |
| `take` | 1–9 | set active take on the target items |
| `ripple_delete` | — | remove time-selection contents, else the target items, closing the gap |
| `select_cursor_item` | — | TIMELINE tap: exclusively select the item under the cursor (tap again: deselect) |
| `zoom_cursor` | — | TIMELINE turn: zoom the arrange view in (CW) / out (CCW) around the edit cursor (view change — no undo block) |
| `select_track` | 1–9 | toggle selection of the item under the cursor on track N (rest of the selection untouched; no item under the cursor → no-op) |
| `volume_readout` | — | message box: the first target's volume in dB |
| `smooth_cut` | — | crossfade within the time selection (40916); none → no-op |
| `place_on_top` | — | duplicate the targets onto one new track above |
| `isolate_tracks` / `solo_tracks` | — | view toggles (also feed the LED bar) |

## 7. Wheel routing (`WheelDispatcher`)

`plugin/src/wheel_dispatcher.h`: one pure class decides where each tick's
wheel input goes. Priority order:

1. **Active hold** — a held key beats every mode: the delta becomes the
   held key's `HoldRotate`; jog/scroll/shuttle see nothing.
2. **Jog** — delta passes through to `JogEngine`: the playhead scrubs
   screen-space-constant — `dt = (delta / 360) * (k_px / pxPerSec)` per
   tick (`k_px` = `wheel.pixels_per_detent`, `pxPerSec` = live
   `GetHZoomLevel`), so one detent always moves `k_px` screen pixels at any
   zoom. SCRL shares the same scale for its view scroll.
3. **Scrl** — `Router::OnWheel`: the arrange view scrolls by the same
   per-detent pixels (read–shift–set of `GetSet_ArrangeView2`); the engine
   sees 0, so scrolling never moves the playhead.
4. **Shtl** — the absolute protocol position drives
   `Router::OnShuttleOffset`; re-applied every tick (absolute input: no
   integration).

## 8. Held-verb (turn) mechanism

One hold = one held verb: `Begin` at hold start, `Step(delta)` per wheel
delta, `End` on release. Targets are resolved **once** at `Begin` via
`TargetItems`, the selection snapshot is taken then and restored at `End`,
and the whole hold runs in **one undo block** (no targets → trace
`target none`, no undo block). Step size: `step=frame | grid | sample |
<N>ms` (default `frame` = 1/fps); accumulation is
`hold_ticks_per_step` wheel ticks per step (default 4; remainder
carries, sign = direction). `nudge` variants: `move` (position),
`trim_in`/`trim_out` (edges, content stays), `slip_src`/`slip_dest`
(source vs destination offsets), `volume` (0.5 dB steps), `sel_in`/`sel_out`
(time-selection edges), `roll` (edit point between neighbours),
`fade_in`/`fade_out` (crossfade lengths). Edges clamp (length ≥ one step,
start offset ≥ 0); nudges are not ripple-aware.

## 9. Targeting (no select button)

The Speed Editor has no select button, yet most item actions act on
*selected* items. Rule in `TargetItems` (`plugin/src/target.{h,cpp}`), in
order:

1. Items already selected → them, unchanged (REAPER-native).
2. Else the item on the *target track* containing the edit cursor —
   `pos <= cursor < pos+len`, or the cursor exactly at the end of that
   track's last item; a boundary belongs to the item starting there.
   Target track = first selected track, else last-touched track.
3. Else nothing: the verb no-ops and traces `target none`.

`WithTargetSelected` snapshots the selection, selects the targets *only*
in case 2, runs the verb, and restores — net selection change is zero, so
the device never silently changes the user's selection. Explicit selection
is still on the device: TIMELINE tap / double (§6); TIMELINE turn is the
`zoom_cursor` view zoom.

## 10. LED status bar

Each main-panel key's LED bit reflects the **state read** of the verb
bound to that key: for every LED-bearing key, take the first bound
gesture in the priority **Tap → Double → Turn → Press** and light the
bit iff that verb's state read exists and returns true against live
state. Unbound keys, or bindings to verbs without a read (one-shot
actions, named `action <name>`), stay dark. There is no key-identity
table and no hardcoded action list in the mask — the normative design,
types, and the key→bit table are in
[`docs/notes/led-binding.md`](notes/led-binding.md) (`BindingLedMask` in
`led_state.{h,cpp}`, polled ≤250 ms, pushed only on change; the device
never writes these bits except its own mode LEDs).

**Seeded action reads** (`ActionStateReads()`, `verb_registry.cpp`):
`action <id>` has a state read only for the four ids below; with the
default `bindings.ini` each lights its own key's LED:

| read | command | lit when | default binding |
|---|---|---|---|
| SNAP | 1157 | snap on (`ToggleState(1157)`) | `SNAP.tap = action 1157` |
| CUT | 40310 | per-track ripple on (`RippleMode() == 1`) | `CUT.tap = action 40310` |
| DIS | 40311 | ripple all tracks on (`RippleMode() == 2`) | `DIS.tap = action 40311` |
| LIVE_OWR | 41117 | trim-below on (`ToggleState(41117)`) | `LIVE_OWR.tap = action 41117` |

Other seeded reads: `select_track <N>` — the CAM keys, true iff the
item on track N under the edit cursor is selected (default
`CAM<n>.tap = select_track <n>`; the LED lights as long as that item is
selected — without the key being pressed — and goes dark once it is
deselected or the cursor leaves it); `take <N>` — true iff
`SharedActiveTake(project) == N`, so a key bound to `take N` lights
while the selected items **all** share take N (mixed or none selected →
dark); `isolate_tracks` — `ctx.isolate_active`
(`VIDEO_ONLY.tap`); `solo_tracks` — `ctx.solo_active`
(`AUDIO_ONLY.tap`). Every other verb has no read → its key's LED stays
dark.

Plus the three jog-mode LEDs (report 0x04: JOG / SHTL / SCRL), set when
the wheel mode changes or the device (re)connects — a separate path,
untouched.

## 11. REAPER actions (action list: "Speed Reaper")

- **reload bindings** — re-read `bindings.ini`, report the count and any
  bad lines to the console.
- **open bindings file** — ensure the file exists, open it in the OS
  editor.
- **show bindings** — print the live (key, gesture) → verb table.
- **echo fired bindings (toggle)** — while on, each fired binding prints
  `KEY.gesture → verb` to the console (the discovery aid).
- **learn** — `PromptForAction` picks a REAPER action; the next device
  gesture (press / tap / double) becomes the binding for it and is
  written into the file, then reloaded. ESC or 10 s cancels; a turn
  gesture is refused (turn handles are nudge verbs, not plain actions).

## 12. Shuttle law

`ShuttleRate(offset)` (`plugin/src/shuttle.{h,cpp}`), pure: input is the
absolute jog offset in [−4096, 4096]. Dead band |offset| < 40 → 0;
±1.0 at the dead-band edge; ±8.0 at |offset| ≥ 4096; smoothstep in
between: `rate = 1 + 7·s²(3−2s)` with `s = (|offset|−40)/4056` clamped to
[0,1], signed by the offset. Applied every tick from the *present*
offset (no integration).

REAPER's playrate API only reaches +0.25…+4.0 (master clamp; the requested
rate is never trusted — `Master_GetPlayRate` read-back is the source of
truth), so: **forward** goes through `CSurf_OnPlayRateChange(min(rate,4.0))`
(the 4…8× outer half is flat at 4.0× in transport terms); **reverse** is
synthesized as stepped seeks — while the rate is negative and the
transport plays, the playhead steps back `|rate|·dt` per 8 ms engine tick
(`SetEditCurPos`, clamped ≥ 0). Dead band → hard stop (pause, no rewind).
STOP/PLAY in SHTL = hard stop, rate 1.0, exit to the prior mode; JOG/SCRL
press = symmetric exit (rate 1.0, mode switches back). Reverse scrub
sound is unmeasured (headless oracle) — see the known limitations in
`docs/archive/plugin-design-m4-shuttle-impl.md`.

## 13. Where to look next

- `docs/speed-editor-capabilities.md` — device protocol facts
- `docs/reaper-api-subset.md` — the REAPER API the plugin uses
- `docs/plugin-oracle-design.md`, `docs/windows-oracle-design.md` — the
  headless test harnesses behind `tools/check`
- `docs/reaper-dev-env.md` — building
- `docs/reaper-actions.tsv` — REAPER action ids (the only id source)
- `docs/archive/` — the milestone design docs that shaped, then
  preceded, this file
