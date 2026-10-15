# SMOKE-TEST.md — release smoke test

One ordered checklist a human runs on real hardware in ≤ 20 minutes, from
the release zip to a verified REAPER plugin. Run it top to bottom; every
step has an action, an expected result, and a checkbox. The speedcli
device tests (Tests 1–4) live in the appendix — they check the USB layer
independently of the plugin and need a dev build.

## Prerequisites (once per machine)

1. **Device**: Blackmagic Speed Editor available, USB-C cable at hand
   (plug it in at step 1 of the checklist).
2. **Permissions (Linux)** — the editor appears as `/dev/hidrawN`. The
   release zip ships `99-blackmagic-speed-editor.rules`; install it with
   `sudo cp 99-blackmagic-speed-editor.rules /etc/udev/rules.d/`, then
   `sudo udevadm control --reload && sudo udevadm trigger` (or:
   `sudo usermod -aG input $USER` and log out/in). No `/dev/hidraw*` at
   all → `sudo modprobe hidraw`. Windows: none needed.
3. **Exclusivity**: only one host app may claim the device at a time.
   Close DaVinci Resolve (and any other tool touching the editor,
   including `speedcli`) while running the checklist.
4. **REAPER**: an installed REAPER (≥ 7.5). The checklist uses a **clean
   profile** so the first-run defaults are what you are testing — see
   step 1.

## Release checklist

Setup for the item verbs (steps 5–9): one REAPER project with **two audio
items (A then B) on one track meeting at a hard boundary**, A with
**three takes** (drop the same file three times into the take lanes) cut
from a longer source so the SLIP SRC slide is audible, and a **second
track** with an item on it (for the view buttons). Ripple editing **off**
(Options → Ripple Editing → Off). Keep the console open
(View → *Show console window*) — several expected results are console
lines.

### 1. Install from the release zip into a clean REAPER profile

**Action**: unpack `speed-reaper-<version>-<platform>.zip` — it contains
`reaper_speed_reaper.so` (Linux) / `.dll` (Windows), `README.md`,
`LICENSE`, `docs/bindings.md`. Start REAPER with a **fresh config**
(Linux: `reaper --dir /tmp/reaper-smoke`, or first-launch portable mode;
Windows: move `%APPDATA%\REAPER` aside first). Copy the `.so`/`.dll` into
that profile's `UserPlugins/` and start (or restart) REAPER.

**Expected**: Options → Extensions lists *Speed Reaper*; the console
prints a `Speed Reaper <version>` line on load; REAPER is otherwise
pristine (no prior `speed-reaper/` folder in the resource path).

- [ ] pass

### 2. Device found

**Action**: with REAPER running, plug the Speed Editor in over USB-C.

**Expected**: console shows `Speed Reaper: connected (serial …)` within a
few seconds — no action-list step, no configuration.

- [ ] pass

### 3. Wheel modes + LEDs

**Action**: (a) with transport stopped, jog the wheel slowly, faster,
reverse; (b) press **SCRL**, jog again, then **JOG**; (c) press **SHTL**,
start playback with STOP/PLAY and deflect the wheel forward of center,
sweep to ~¼ and ~½ deflection, then park at center.

**Expected**: (a) the **JOG** LED is lit on connect (the plugin set it);
the edit cursor follows 1:1 (~20 s of timeline per revolution) and
**stops the instant the wheel stops**; the arrange does not scroll.
(b) LED switches JOG off / **SCRL** on; the wheel scrolls the arrange
**horizontally** at roughly the same on-screen speed as the cursor's jog;
the edit cursor does not move; mid-spin mode switches flip behavior and
LED together. (c) **SHTL** LED lights (JOG off); playback rate climbs
with deflection (≈ 2× at ~¼, capped at 4× from ~½ on forward); at exactly
center playback **stops and stays stopped** (dead band); the SHTL LED is
the only lit mode LED in SHTL, never JOG/SCRL.

- [ ] pass

### 4. Transport

**Action**: in JOG press **STOP/PLAY** twice; re-enter SHTL with the
wheel deflected, playing, then press **STOP/PLAY** again.

**Expected**: in JOG, STOP/PLAY toggles play/stop each press (40044). In
SHTL, STOP/PLAY is a **hard stop** and the mode returns to the one you
came from (JOG lit) — never a play toggle.

- [ ] pass

### 5. Split / trim / ripple group

**Action**: cursor inside A, A selected: (a) tap **SPLIT**; (b) tap
**IN**, jog the cursor, tap **OUT**; (c) double-tap **IN**; (d) tap
**ESC** once (clears any leftover time selection); (e) cursor inside a
split half: tap **TRIM IN**, then **TRIM OUT**; (f) ripple still off,
cursor inside an item: tap **RIPL DEL**; (g) tap **TIMELINE** with
nothing selected and cursor inside an item, tap again, then double-tap.

**Expected**: (a) the item splits at the cursor (40012). (b) the arrange
shows a blue time selection spanning the two cursor positions (taps are
deferred ~300 ms — the double-tap window). (c) the time selection is
cleared (40635). (d) same, single ESC tap clears. (e) the item's left,
then right edge jumps to the cursor (40511/40512 — destructive; undo or
test on a project copy). (f) the item under the cursor is **deleted and
the gap closes** (later item shifts left) **with ripple off** — the
plugin ripples the delete itself; check Options → Ripple Editing and the
mode is still Off afterwards. (g) first tap selects the cursor item
exclusively, second tap deselects, double-tap deselects all items
unconditionally (40289).

- [ ] pass

### 6. Holds (hold + wheel)

**Action**: with the two meeting items selected: (a) hold **SPLIT**, turn
the wheel right 8 ticks, release; (b) hold **TRIM OUT**, turn left 8
ticks, release; (c) select only A (the 3-take item from a longer
source), play it once, stop, hold **SLIP SRC**, turn right 48 ticks,
release, play it again; (d) select it (volume at 0 dB), hold
**AUDIO LEVEL**, turn right 40 ticks, release, check Item Properties;
(e) for each of a–d still in the undo history, **double-tap ESC**.

**Expected**: (a) each 4 wheel ticks the item jumps **exactly one project
frame** — discrete, not a drag — and the neighbor does not follow (hold
is not ripple-aware); (b) the item's right edge moves one frame per step;
(c) the item's timeline edges are unchanged but the **audible content has
slid ~0.4 s later in the source** (clamped at the source start if you
turn left too far); (d) Item Properties shows **+5.00 dB** (10 steps ×
0.5 dB), and turning left clamps at 0 dB; (e) **one ESC double-tap
reverts the whole hold** — the undo history has exactly one
`Speed Reaper: …` entry per hold gesture, and the double-tap does not
also clear any active time selection.

- [ ] pass

### 7. Takes

**Action**: select A (3 takes), cursor inside: (a) tap **CAM 2**, then
**CAM 3**, then **CAM 1**; (b) click empty arrange space (nothing
selected), jog the cursor back inside A, tap **CAM 2**; (c) single-tap
**AUDIO LEVEL**; (d) double-tap **AUDIO LEVEL**.

**Expected**: (a) the active take lane switches 2 → 3 → 1 on each tap;
the single-take item B is **untouched** (targets with fewer takes keep
theirs); each switch is one undo step (ESC double-tap goes back one).
(b) with nothing selected the verb acts on the item under the cursor.
(c) the console prints `Volume: <x.x> dB` for the first target item — no
marker, no undo entry. (d) a **project marker is inserted at the edit
cursor** (40157); the cursor itself does not move.

- [ ] pass

### 8. View

**Action**: target = A (selected or under the cursor), no tracks soloed,
all visible: (a) tap **VIDEO_ONLY**, watch, tap again; (b) tap
**AUDIO_ONLY**, watch, tap again; (c) select an item and tap
**PLACE ON TOP**; (d) tap **FULL_VIEW**, then again.

**Expected**: (a) first tap: every track except A's is hidden (isolate
view); second tap: the previous per-track visibility is restored exactly.
(b) first tap: A's track is soloed; second tap: previous solo state
restored. Both toggles make **no undo entries** — only the re-tap
restores. (c) a new track appears directly above with a copy of the item
at the same position; the original stays put; selection returns to the
original; ESC double-tap removes track + copy in one undo step
(`Speed Reaper: place on top`). (d) the whole REAPER window goes
fullscreen (40346); the second tap returns to the normal window.

- [ ] pass

### 9. Crossfade keys

**Action**: with A and B meeting at the boundary (recreate if step 5's
split disturbed the layout): (a) set a time selection that **contains the
boundary** (IN, jog past it, OUT), tap **DIS**; (b) tap **CUT**; (c)
leave a second boundary selection in place and tap **TRANS_DUR**; (d)
make two items **overlap** on the same track (hold SPLIT on one and drag
it onto the other), select them, tap **TRANS**; (e) tap **DIS** with
**no** time selection.

**Expected**: (a) auto-fades appear at the boundary inside the selection
(40916) — the hard cut plays as a crossfade. (b) the items' fade in/out
are **removed** (41193) — the crossfade is gone. (c) the selection's
crossfade length is set to REAPER's default (43776). (d) the overlapping
selected items get crossfades (41059). (e) **nothing happens** — without
a time selection the verb no-ops rather than guessing.

- [ ] pass

### 10. Bindings actions

The file is `<resource path>/speed-reaper/bindings.ini` (find the path:
Options → *Show REAPER resource path in explorer*). All actions are in
the Action List under “Speed Reaper:”.

**Action**: (a) run **show bindings**; (b) toggle **echo fired bindings**
on, tap a bound key (e.g. SNAP — note: SNAP.tap binds `action 1157`,
toggle snapping); (c) run **open bindings file**, change `SNAP.tap` to
`SNAP.tap = action 40157` and add a junk line `FOO.tap = action 1`,
save, run **reload bindings**; (d) run **learn**, pick any action in the
picker (e.g. "Transport: Play"), then tap a currently-unbound key on the
device (e.g. APPND); (e) restart REAPER (step 12) and re-test the learned
key.

**Expected**: (a) the console lists every binding grouped, plus an
`Unbound:` line for the shipped-unbound keys. (b) each press prints
`KEY.gesture → verb args` in addition to firing; toggling echo off stops
the messages. (c) console: `Speed Reaper: bindings loaded n=<count>` and
`bindings.ini:<line>: …` for the junk line (parsing continues past it);
SNAP now inserts a marker at the cursor. (d) the console prompts "press a
device key + gesture…"; the file gains `APPND.tap = action …` in place
of / appended to the existing line (comments preserved); the tapped key
runs the action immediately; ESC or a 10 s timeout cancels and changes
nothing; hold+turn is refused. (e) after the restart the learned binding
is still active — the file survives and is the source of truth.

- [ ] pass

### 11. Unplug / replug

**Action**: with REAPER connected, unplug the USB cable; wait a few
seconds; replug.

**Expected**: console shows `Speed Reaper: disconnected`, REAPER stays
fully responsive (the device is just gone); replugging shows
`Speed Reaper: connected (serial …)` **without restarting REAPER**, and
the current wheel mode + its LED are restored automatically — no button
press needed.

- [ ] pass

### 12. REAPER restart

**Action**: quit REAPER fully, relaunch (device plugged in).

**Expected**: `Speed Reaper: connected (serial …)` on startup — no manual
step; the learned binding from step 10(d) still fires; and one last
reset: delete `speed-reaper/bindings.ini`, restart REAPER — the file is
**recreated from the built-in default** (your edits are gone, one
commented line per key), while an existing file is never overwritten on
startup.

- [ ] pass

**Pass bar**: every checkbox ticked — the plugin installed cleanly from
the zip into a fresh profile, found the device, drove the wheel modes and
LEDs, fired every default binding group correctly, and the binding file
behaved as the single source of truth (edit + reload, learn, restart
persistence, delete → default).

## Troubleshooting

- **No `Speed Reaper: connected`** → device unplugged, permissions
  (Prerequisite 2), or another app holding the device (Prerequisite 3).
- **Buttons report but nothing happens in REAPER** → check the console:
  an `Unbound:` or `bindings.ini:<line>: …` line means the file was
  edited; delete it (step 12) to restore the default.
- **Authentication failed / flaky device** → power-cycle the editor
  (unplug ~5 s), make sure nothing else touched it, replug.
- Protocol reference: `docs/speed-editor-capabilities.md`; bindings
  reference: `docs/bindings.md` (also in the zip).

## Appendix — speedcli device tests (Tests 1–4)

These check the USB layer without the plugin. They need a dev build from
the repo (outside the release zip): `make -C src` → `build/speedcli`
(inside the nix dev env). Run with **REAPER closed** (exclusivity).

### Test 1 — `scan`: the device is visible on the USB bus

```sh
./build/speedcli scan
```

**Expected (plugged in, exit 0):**
```
Speed Editor found:
  path   : /dev/hidrawN
  serial : ...
```
**Expected (not plugged in, exit 1):** `No Speed Editor (VID 0x1EDB PID 0xDA0E) on the USB bus.` plus a hint about `ls /dev/hidraw*`.

### Test 2 — `listen`: inputs decoded + outputs driven from buttons (default command)

```sh
./build/speedcli listen
# add -v in front to hexdump every raw input report
```

**Expected at startup:**
```
device : Speed Editor (serial ...)
auth   : OK (listen)
wheel  : mode 0 (jog/relative) set, JOG LED on
try    : spin the wheel, press SHTL/JOG/SCRL (wheel mode + LED),
         SNAP (toggles its LED), any other button, unplug USB
         (battery report). ESC or Ctrl-C exits.
```
Note: the **JOG LED is lit** immediately — the CLI set wheel mode 0 and its
LED on the device (first output control).

Then, on the physical device:

| Do this | Expect on the terminal |
|---|---|
| Spin the wheel | `wheel  : delta=+5  mode=0 RELATIVE_0 (jog)`-style lines, one per report; faster spins → larger magnitudes, direction = sign |
| Press **SHTL** | `button : down  SHTL ...` then `control : wheel mode 1 (shuttle/absolute) + SHTL LED on` — and the **SHTL LED** lights on the device |
| Press **JOG** / **SCRL** | same pattern → wheel modes 0 / 2, their LEDs lit |
| Press **SNAP** | `control : SNAP LED on` / `off` on each press — the SNAP button's LED toggles (main-panel LED, report 0x02) |
| Press any other button (e.g. **CUT**, **CAM 3**, **IN**) | `button : down  CUT (0x000F)` (name decoded from the keycode); releasing prints `button : up` |
| Press **STOP/PLAY** | its name + `note : STOP/PLAY pressed (the CLI has no transport target)` |
| Hold **two** buttons at once | both keycodes appear — reports carry the full held set, no auto-repeat |
| Unplug the USB cable | `battery: charging=0 level=NN%` (battery report 0x07), then `device : disconnected` and a clean exit |
| Press **ESC** or Ctrl-C | `bye : closed, all LEDs off` (device LEDs cleared on close) |

Pass bar: every input type is decoded with a human-readable name, and at
least the wheel-mode buttons + SNAP demonstrably change *device* outputs
(look at the physical LEDs while doing this).

### Test 3 — `demo`: walk all 21 LEDs

```sh
./build/speedcli demo
```

**Expected:** ~400 ms per step —
1. each of the 18 main-panel LEDs lights in turn (`bit 0 CLOSE_UP` …
   `bit 17 AUDIO_ONLY`) while the other three (jog) LEDs are off;
2. the 3 jog-mode LEDs light in turn (JOG, SHTL, SCRL);
3. all 21 LEDs on together for 1 s, then everything off and `demo   : complete — all LEDs off`.

Pass bar: watch the physical panel — every printed LED actually lights at
its moment, nothing stays stuck on afterwards.

### Test 4 — `mode N`: set the wheel protocol mode and exit

```sh
./build/speedcli mode 1
```

**Expected:** `wheel  : mode 1 ABS_CONT (shuttle) set, jog LEDs updated`,
SHTL LED lit on the device, exit 0. Try `0`, `2`, `3` (mode 3 →
`jog LEDs off (no button)`). After `mode 1`, spinning the wheel reports
absolute positions (run `./build/speedcli listen` — press SHTL there, spin
the wheel, compare delta semantics with mode 0).

### Exit codes

| code | meaning |
|---|---|
| 0 | ok |
| 1 | no device / could not open (see Prerequisite 2–3) |
| 2 | authentication failed (challenge/response on report 0x06) |
| 3 | HID write failed |
| 64 | usage error |
