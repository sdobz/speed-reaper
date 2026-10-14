# SMOKE-TEST.md — manual smoke tests

Hand-run tests for the speed-reaper prototypes. Each test lists the command
and the expected result. Run everything from the repo root, inside the nix
dev env.

## Prerequisites (once per machine / session)

1. **Device**: Blackmagic Speed Editor plugged in over **USB-C**.
2. **Dev env**: this repo uses direnv (`.envrc` → `flake.nix`). Either
   `cd` into the repo and run `direnv allow` once (the shell enters
   automatically), or run commands via `nix develop --command ...`.
   Verify: `pkg-config --modversion hidapi-hidraw` → `0.15.x`.
3. **Permissions** — the editor appears as `/dev/hidrawN`. Pick one:
   - **udev rule (recommended)** —
     `sudo tee /etc/udev/rules.d/99-blackmagic-speed-editor.rules`:
     ```
     SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1edb", ATTRS{idProduct}=="da0e", MODE="0666"
     ```
     then `sudo udevadm control --reload && sudo udevadm trigger`, and
     unplug/replug the editor.
   - or: `sudo usermod -aG input $USER`, then log out and back in.
   - quick-and-dirty: run the commands with `sudo` (fine for a smoke test).
   Verify: `ls -l /dev/hidraw*` shows the node you can read.
   (No `/dev/hidraw*` at all → the kernel `hidraw` module is missing:
   `sudo modprobe hidraw`.)
4. **Exclusivity**: only one host app may claim the device at a time.
   Close **DaVinci Resolve** (and any other tool touching the editor)
   before each test. If Resolve holds it, `listen` fails to open.

## Build

```sh
make -C src        # → build/speedcli
```

Expected: `built ../build/speedcli` and no compiler warnings.
Clean: `make -C src clean`.

## Test 1 — `scan`: the device is visible on the USB bus

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

## Test 2 — `listen`: inputs decoded + outputs driven from buttons (default command)

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

## Test 3 — `demo`: walk all 21 LEDs

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

## Test 4 — `mode N`: set the wheel protocol mode and exit

```sh
./build/speedcli mode 1
```

**Expected:** `wheel  : mode 1 ABS_CONT (shuttle) set, jog LEDs updated`,
SHTL LED lit on the device, exit 0. Try `0`, `2`, `3` (mode 3 →
`jog LEDs off (no button)`). After `mode 1`, spinning the wheel reports
absolute positions (run `./build/speedcli listen` — press SHTL there, spin
the wheel, compare delta semantics with mode 0).

## Exit codes

| code | meaning |
|---|---|
| 0 | ok |
| 1 | no device / could not open (see Prereq 3–4) |
| 2 | authentication failed (challenge/response on report 0x06) |
| 3 | HID write failed |
| 64 | usage error |

## Troubleshooting

- **"could not open the Speed Editor"** → device missing (Test 1),
  permissions (Prereq 3), or Resolve holding it (Prereq 4).
- **authentication failed** → re-run; if persistent, power-cycle the device
  (unplug ~5 s) and make sure no other host touched it mid-handshake.
- **buttons report but no LEDs change** → you are looking at the wrong
  device / another app stole the device (exclusivity).
- Protocol reference: `docs/speed-editor-capabilities.md`.

## REAPER plugin: hello world (smooth playhead control)

The `plugin/` extension (no Speed Editor hardware needed — it uses a
synthetic wheel source standing in for the driver) registers three actions
that move the playhead through the same `IWheelSource → JogEngine →
CSurf_ScrubAmt` pipeline the real editor will use.

### Build

```sh
cmake -S plugin -B build/plugin -G Ninja
cmake --build build/plugin
```

**Expected:** `build/plugin/reaper_speed_reaper.so`, no compiler warnings.

### Install (once)

```sh
cp build/plugin/reaper_speed_reaper.so ~/.config/REAPER/UserPlugins/
```

(`UserPlugins/` or `Plugins/` under the REAPER resource path — find it via
Options → *Show REAPER resource path in explorer* if REAPER is installed
portably instead of via nixpkgs.)

**Restart REAPER** (or Options → Extensions — the plugin must be loaded for
its actions to exist). Verify: Options → Extensions shows
*"Speed Reaper (hello world)"*.

### Test 5 — actions exist

Actions → Show Action List → search `Speed Reaper`. **Expected:** three
actions:
- `Speed Reaper: jog demo (smooth playhead)`
- `Speed Reaper: scrub forward 0.5 s`
- `Speed Reaper: scrub back 0.5 s`

### Test 6 — jog demo: smooth playhead control

1. Open any project (an empty one is fine), click somewhere in the timeline
   to place the edit cursor, make sure transport is **stopped**.
2. Run *"Speed Reaper: jog demo (smooth playhead)"* (double-click or the
   Run button).

**Expected:** the edit cursor/playhead **glides forward** over ~1.2 s
(accelerates, cruises, coasts to a stop) — it does not jump. Run it again
while it is still moving → it stops. Run it a third time → a new turn
starts from the current position. The playhead ends ~0.5 s ahead of where
it started.

### Test 7 — one-shot nudges (sink path without the source)

Run *"Speed Reaper: scrub forward 0.5 s"* → playhead jumps +0.5 s in one
step (no smoothing — this bypasses the wheel). Run *"scrub back 0.5 s"* →
returns to the previous position. Both work while stopped; while playing,
`CSurf_ScrubAmt` scrubs the play position (audio tracks the scrub — that
is REAPER's built-in scrub behaviour, not the plugin's).

### Uninstall

Delete `~/.config/REAPER/UserPlugins/reaper_speed_reaper.so`, restart
REAPER.

### Troubleshooting

- **Actions missing** → plugin not loaded: check Options → Extensions
  (it appears after a REAPER restart), and that the `.so` is in the
  resource path (Options → Show REAPER resource path in explorer).
- **Action runs but nothing moves** → REAPER version too old or wrong
  section: the actions live in the *main* section; run them from the main
  action list, not a MIDI editor list.
- **Build fails with `gdk/gdk.h: No such file`** → you are outside the nix
  dev env (`gdk-3.0` pkg-config module missing); re-run in `nix develop`.
- **Windows**: build via the flake cross path, not the dev shell —
  `nix build .#reaper-plugin-windows` (see the next section and
  `CROSS-PLATFORM.md` at the repo root).

### Windows build (cross, from the Linux dev env)

This project is cross-platform — see `CROSS-PLATFORM.md` for the matrix.
The same plugin sources build a Windows `.dll` via the flake (MinGW
toolchain; works because the plugin's REAPER surface is C-ABI only):

```sh
nix build .#reaper-plugin-windows
file $(nix path-info .#reaper-plugin-windows)/reaper/reaper_speed_reaper.dll
```

**Expected:** `PE32+ DLL` / x86-64, no compiler warnings.

To test on a Windows machine: copy `reaper_speed_reaper.dll` to
`%APPDATA%\REAPER\UserPlugins\`, start REAPER, and run Tests 5–7 above
identically (Options → Extensions should list *"Speed Reaper (hello
world)"*; the action names are the same). The DLL statically links its
C++ runtime — no Visual C++ redistributable needed.

If a future version of the plugin implements REAPER C++ interface
classes (vtables REAPER instantiates), the MinGW path stops being safe —
build on Windows with MSVC instead and update `CROSS-PLATFORM.md`.

## M1 — wheel drives the playhead

Automated equivalent: `tools/reaper-oracle --layer reaper --scenario hid_wheel`; this hand test only adds real hardware.

1. **Prereqs**: DaVinci Resolve and any running `speedcli` closed (Prerequisite 4); udev rule from Prerequisite 3 installed. Speed Editor plugged in over USB-C.

   **Expected:** `./build/speedcli scan` finds the device; REAPER is not yet running.

2. **Probe the driver** (no REAPER): run `hid_probe --seconds 10` (see `plugin/tools/`) with the device plugged in; while the 10 s window runs, turn the wheel one way, then the other; then run it again with the device unplugged.

   **Expected (plugged in):** `log: connected (serial …)` at startup; turning the wheel changes `wheel: sum`, and the sign flips with direction. **Expected (unplugged):** `log: device not found` and no `wheel: sum` lines. (The tool exits 0 either way after the full window — it is a probe, not a pass/fail gate.)

3. **Build + install the plugin**: reuse the hello-world *Build* and *Install (once)* steps above (`cmake … build/plugin/reaper_speed_reaper.so`, copy to `~/.config/REAPER/UserPlugins/`), then start REAPER with the device still plugged in.

   **Expected:** REAPER console shows `Speed Reaper: connected (serial …)` shortly after startup.

4. **Jog the playhead**: with the edit cursor on the timeline and transport **stopped**, turn the wheel — first slowly, then faster, then reverse — and release it.

   **Expected:** the edit cursor follows the wheel 1:1 — about 20 s of timeline per full revolution (`JogEngine::kSecondsPerRev`, `docs/notes/m1-wheel-scale.md`), so a small fraction of a turn is a small, precise move. It **stops the instant the wheel stops** (no coast, no drift), reversing direction reverses immediately, and a slow turn followed by an equal fast turn the other way returns to the start position.

5. **Hot-unplug**: while REAPER is running and connected, unplug the USB cable; then replug.

   **Expected:** console shows `Speed Reaper: disconnected`; REAPER stays responsive (transport, actions, UI all work — the device is just gone). Replugging shows `Speed Reaper: connected (serial …)` again **without restarting REAPER**.

6. **Restart REAPER** with the device attached (quit fully, relaunch).

   **Expected:** `Speed Reaper: connected (serial …)` on startup — the plugin reconnects across REAPER restarts with no manual step.

7. **Observation** (record the answers in `docs/notes/m1-hid-observations.md`): with REAPER holding the device —
   - press a few Speed Editor buttons (e.g. **CUT**, **SNAP**, **CAM 3**): do they type characters or trigger REAPER shortcuts in any app (OS-level HID keyboard leak)?
   - run `./build/speedcli listen`: does it still open the device, or fail with "could not open" (exclusivity)?

   **Expected (hypothesis to confirm):** no visible keystrokes/shortcuts anywhere; `speedcli listen` fails to open while REAPER holds the device (exclusivity), and succeeds again after REAPER is closed.

## M2 — buttons and wheel modes

Automated equivalent: `tools/reaper-oracle --layer reaper --scenario buttons`; this hand test only adds real hardware.

Start from the M1 state: Speed Editor plugged in, plugin installed (M1 step 3), REAPER running with `Speed Reaper: connected (serial …)` in the console, edit cursor on the timeline.

1. **Startup / JOG (default mode)**: transport **stopped**; turn the wheel — slowly, faster, then reverse — and release.

   **Expected:** the **JOG** LED is lit on the device right after startup (the plugin set it — you did not press anything); the edit cursor follows the wheel exactly as in M1 (~20 s of timeline per revolution), stops the instant the wheel stops. The arrange view does **not** scroll.

2. **SCRL: horizontal scroll, cursor frozen**: press **SCRL** once. Turn the wheel in one direction, then the other. Then press **JOG** and turn again.

   **Expected:** the LED switches JOG off / **SCRL** on. While SCRL is lit, turning the wheel scrolls the arrange view **horizontally** (direction follows the wheel) and the edit cursor does **not** move. The view travels at roughly the **same on-screen speed as the cursor** in JOG (SCRL scrolls the same 20 s of timeline per wheel revolution the cursor moves — at any zoom level; it should not fly across the screen). JOG brings the cursor motion back. Mode-switch while the wheel is still spinning (keep it turning, tap SCRL, then JOG): the LED and the behavior flip together — the motion changes from cursor to scroll and back immediately, mid-turn.

3. **STOP/PLAY**: press **STOP/PLAY**.

   **Expected:** playback starts. Press again: it stops. Repeat with SCRL active (SCRL LED lit) — it starts/stops transport there too, and the wheel still only scrolls while in SCRL.

4. **SHTL: deliberately inert**: press **SHTL** in JOG, then in SCRL.

   **Expected:** nothing happens — no shuttle motion, no mode change, **no LED change** (the SHTL LED stays off, the current mode's LED stays lit). Documented as expected; shuttle handling lands in M4.

5. **Unplug/replug in SCRL**: with the SCRL LED lit, unplug the USB cable, wait a few seconds, replug.

   **Expected:** console shows `Speed Reaper: disconnected`, then `Speed Reaper: connected (serial …)`; after reconnect **SCRL is still the active mode** and its LED is lit (mode + LEDs are re-sent automatically on the reconnect edge — no button press needed). Wheel still scrolls, cursor still frozen.

6. **Tuning note — scroll speed**: SCRL deliberately shares the jog scale (`JogEngine::kSecondsPerWheelUnit` in `plugin/src/jog_engine.h`), so it matches the cursor's on-screen speed at any zoom by construction. If you want SCRL to travel faster/slower *than the cursor*, that is a deliberate divergence — record the desired ratio and the feel in `docs/notes/m2-observations.md` (a `scroll.speed_factor` parameter is its likely home, `docs/plugin-configuration.md` §2.4).

Pass bar: the jog LEDs are always truthful (exactly one of JOG/SCRL lit, SHTL never lit), SCRL scrolling never moves the edit cursor, STOP/PLAY toggles transport in both modes, and the active mode survives an unplug/replug.

## M3 — edit verbs

Automated equivalent: `tools/reaper-oracle --layer reaper --scenario verbs` (also the check that settled the command ids — see `docs/notes/m3-observations.md`); this hand test only adds real hardware.

Setup: Speed Editor plugged in, plugin installed and connected (M1 step 3 / M2 start state). In REAPER, load or save a project with **two audio items on one track** (any two audio files dropped onto a track work). Select one item (single click). Put the **edit cursor inside that item**, not on an edge.

1. **SPLIT**: with the item selected and the edit cursor inside it, tap **SPLIT**.

   **Expected:** the item is split into two at the edit cursor — exactly what REAPER's own `S` does (console trace: `gesture 2f00 tap`, `cmd 40012`). No split when the cursor is off any item.

2. **IN / OUT / clear**: tap **IN**, move the edit cursor somewhere else (wheel in JOG), tap **OUT**.

   **Expected:** IN sets the **start** of the time selection at the cursor's position at press time; OUT (with the cursor moved) sets the **end** — the arrange view shows the matching blue time selection spanning both points. **IN or OUT taps are deferred ~300 ms** (double-tap window) before the command fires — a visible but tolerable delay; do not double-tap while checking a single tap. Double-tap **IN** (or **OUT**): the time selection is **cleared**. A single **ESC** tap also clears it (`cmd 40635`).

3. **TRIM IN / TRIM OUT**: re-select an item (or use one of the split halves), put the edit cursor inside it, tap **TRIM IN**, then with a cursor again inside the same (now shortened) item tap **TRIM OUT**.

   **Expected:** TRIM IN deletes everything **left** of the cursor — the item's start jumps to the cursor, length shortens from the left (`cmd 40511`); TRIM OUT does the mirror image, the item's end jumps to the cursor (`cmd 40512`). Undo (`Ctrl+Z`) restores the item; it is destructive, so test on a copy of the project or be ready to undo.

4. **RIPL DEL with and without ripple**: with an item selected, tap **RIPL DEL** with REAPER's ripple editing **off** — undo; re-select the item. Now switch ripple editing on — **and verify it is on before tapping**: Options menu → **Ripple Editing** submenu → *Per-track* (or `Alt+P` cycles Off → Per-track → All tracks; on REAPER ≥ 7.35 the toolbar's ripple button left-click toggles and **right-click cycles**, so trust the Options menu). Put the cursor between the two items, select one, tap **RIPL DEL** again.

   **Expected:** either way the selected item is deleted (`cmd 40006`, item count drops). With ripple **off** the gap where the item was remains — the neighboring item does not move. With ripple **on** the gap closes — the later item(s) shift left to close it. This state dependence is REAPER's own semantics (RIPL DEL is `Item: Remove items`; REAPER has no standalone "ripple delete" command — whether a delete ripples is driven by the project's ripple mode), pinned by the automated scenario `tools/reaper-oracle --layer reaper --scenario ripple_del`. Turn ripple off again afterwards.

5. **SNAP + LED**: note REAPER's snap state (toolbar). Tap **SNAP** on the device; tap it again; then toggle snap from **REAPER's own toolbar**.

   **Expected:** each device tap toggles REAPER's snap (`cmd 1157`) and the device's **SNAP LED** matches the resulting state (on lit / off unlit) immediately. Toggling snap from REAPER's toolbar updates the LED **within ~1 s** (the plugin polls REAPER's snap state every 250 ms — `led main=<hex>` trace lines, bit 5). The LED is the source of truth to check: it must never disagree with REAPER for longer than a second.

6. **ESC double-tap = one Undo**: set up an undoable change (e.g. split an item, or set a time selection), then **double-tap ESC**.

   **Expected:** exactly one Undo fires (`cmd 40029`, once — not twice) and the change is reverted. Crucially the double-tap does **not** also fire the ESC tap: if a time selection was active before the double-tap, it **survives** the undo (no `cmd 40635` between the two presses) — the undo gesture must not clear the time selection.

7. **Feel of the gesture windows** (record the answers in `docs/notes/m3-observations.md`): the double-tap window is **300 ms** and the hold threshold **350 ms** (`plugin/src/gesture.h`, `GestureParams`). On the real device: can you double-tap IN/OUT/ESC reliably within 300 ms, or do your natural taps land outside it? Does a deliberate press-and-hold (no rotate needed in M3 — hold+rotate is backlog) feel like a hold at 350 ms, or does it fire as a tap? Note whether 300/350 ms feel right as-is or need tuning.

Pass bar: every verb does exactly what the same REAPER action does (split at cursor, time selection set/cleared, trims at cursor, item delete with correct ripple behavior, snap toggled), the SNAP LED tracks REAPER's snap both ways within ~1 s, and ESC double-tap undoes once without clearing the time selection.

## M5 — targeting

Automated equivalent: `tools/reaper-oracle --layer reaper --scenario target` (the composite command ids and the REAPER-visible results are pinned there); this hand test only adds real hardware. M5 changes the M3 step 4 expectation: RIPL DEL now **always** closes the gap, ripple mode or not — the plugin makes it ripple itself.

Setup: M1 state (device plugged in, plugin connected). In REAPER: a project with **two adjacent audio items on one track** (A then B, no gap between them), **nothing selected** (click empty arrange space to be sure), ripple editing **off** (Options → Ripple Editing → Off). Put the **edit cursor inside the first item (A)**, not on an edge.

1. **RIPL DEL with ripple off — deletes the cursor item and closes the gap**: with nothing selected and the cursor inside A, tap **RIPL DEL**.

   **Expected:** A is deleted **and B shifts left to where A was** — the gap closes even though ripple is off, because the plugin runs the delete inside a temporary per-track ripple block (40310 → 40006 → 40309). Afterwards REAPER's ripple mode is **back to off** — verify in the Options menu; the plugin restores the prior mode and must not leave it on. This supersedes M3 step 4's "gap remains with ripple off": with M5 the implicit-target RIPL DEL always ripples.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

2. **IN / OUT + RIPL DEL — the time selection wins**: set a time selection covering part of an item (tap **IN**, move the cursor with the wheel in JOG, tap **OUT**), then tap **RIPL DEL**.

   **Expected:** RIPL DEL acts on the time selection, not on the cursor's item: everything inside the selection is removed and the gap it left is closed, across all tracks (`40201`). Item targeting (the cursor rule) is only consulted when there is **no** time selection. Clear the selection afterwards (single **ESC** tap).

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

3. **TRIM IN with no selection**: put the cursor inside an item, make sure **nothing is selected**, tap **TRIM IN**.

   **Expected:** the item is trimmed at the cursor — left edge jumps to the cursor, length shortens from the left — exactly as it would be with the item explicitly selected (the implicit target is picked, used for the trim, then discarded). **Nothing is left selected afterwards**: the plugin's temporary selection is restored to "none", unlike a manual REAPER trim which would leave the item selected.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

4. **TIMELINE selection stepping**: with nothing selected and the cursor inside A: tap **TIMELINE**; tap **TIMELINE** again; then hold **TIMELINE** and rotate the wheel — one direction a bit more than a step, then the other — and release.

   **Expected:** first tap: A becomes the **sole** selection (any other selected items are deselected). Second tap with A still the sole selection: **everything is deselected** (the tap toggles). Double-tap **TIMELINE**: deselects all items unconditionally (`40289`) — works even with several items selected. Hold + rotate: for every **20 wheel ticks** the selection moves to the next/previous item on the target track (watch the item shading step from A to B and back), **clamped at the track's ends** (rotating past the last item keeps it selected — no wrap), and the **edit cursor does not move** at any point. If the cursor is under no item, the tap is a no-op (`target none` in the console trace).

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

5. **ESC double-tap undoes a RIPL DEL in one step**: with nothing selected and the cursor inside A, tap **RIPL DEL** (step 1's delete + ripple shift), then **double-tap ESC**.

   **Expected:** the whole composite is undone in **one** step: A is back at its original position and B shifted back behind it — delete and ripple-close are a single undo block, so the one double-tap restores both. One further double-tap ESC does not roll anything back (no phantom extra undo point from the temporary ripple toggles). Ripple mode is still off.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

Pass bar: with nothing selected, RIPL DEL removes the item under the cursor **and** closes the gap with ripple left off, a time selection redirects RIPL DEL to `40201`, TRIM IN works on the implicit target and leaves nothing selected, the TIMELINE tap/double-tap/hold+rotate verbs select/deselect/step exactly as in step 4, and one ESC double-tap fully undoes a RIPL DEL.

## M4 — shuttle

Automated equivalent: `tools/reaper-oracle --layer reaper --scenario shuttle`
(rates, cursor motion, hand-offs — the numbers are judged there). This hand
test covers only what the oracle cannot judge (E2, real audio): **the sound
at 2×/4×/reverse, and the feel** of the curve, the dead band, and parking at
center. (This section supersedes M2 step 4 — SHTL is no longer inert.)

The law, for reference (`ShuttleRate`, `plugin/src/shuttle.cpp`): the wheel's
**absolute** deflection from center maps to a playback rate — `|offset| < 40`
(dead band, ≈ the first 1% of travel) → stop; `±40` → `±1.0×`; full
deflection `±4096` → `±8.0×` by the law, with smoothstep in between (≈ `2×`
at ~¼ deflection, ≈ `4×` at ~½ deflection). Implementation decision
(`docs/plugin-design-m4-shuttle-impl.md`, rule 2): forward goes through
REAPER's master playrate, which REAPER caps at **4.0×** read-back — so the
audible forward range is 1×–4×, flat at 4× from ~½ deflection up. Reverse is
synthesized as stepped seeks (125 steps/s), so it sounds like a fast scrub,
not time-stretched playback.

Setup: M1 state (device plugged in, plugin installed and connected). In
REAPER: a project with **one long, feature-rich audio item** (speech or music
with clear transients — artifacts are easiest to hear there), edit cursor a
third of the way in, transport playing at 1× via **STOP/PLAY** in JOG mode.

1. **Enter SHTL while playing**: with the item playing at 1×, hold the wheel
   deflected **forward just past the center click**, and press **SHTL**.

   **Expected:** the **SHTL** LED lights (JOG goes off); playback continues
   and the rate climbs above 1× as the deflection grows. The edit cursor no
   longer crawls under the wheel — in SHTL the wheel is absolute (deflection
   sets the rate; there is no relative jog).

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_\_\_

2. **Forward rates 2× and 4× (audible)**: keeping SHTL lit, sweep the wheel
   from center to full forward deflection, pausing briefly at ~¼ and ~½
   deflection, and listen.

   **Expected (sound):** at ~¼ deflection it plays ≈ **2×** (both faster and
   higher-pitched — the master playrate does not pitch-shift); at ~½
   deflection ≈ **4×**; beyond ~½ deflection it stays at 4× (the REAPER cap —
   pushing the wheel further does not change the sound, only the law value).
   No digital buzz or crackle *added by the plugin* — any choppiness you hear
   is REAPER's resampling, and is the point of this step: is 2× clean enough
   for review listening, and is 4×? Note both.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_\_\_

3. **Wheel parked at center = silent (dead band)**: while playing in SHTL,
   bring the wheel back to **exactly center** and leave it there.

   **Expected:** playback **stops immediately** — the dead band (`|offset| <
   40`) is a hard stop, not a pause at 1×, and there is **no creep backwards**
   ("no rewind"). The transport is stopped and the rate is back to 1.0, so
   the moment you are at center it is silent and it stays silent while the
   wheel sits there. Note the asymmetric restart: deflecting forward again
   past center does **not** restart playback — while the transport is stopped
   the rate law parks (the wheel still drives the law, but the sink only
   acts while playing). Playback resumes by leaving SHTL (JOG tap) and
   pressing STOP/PLAY, then re-entering SHTL with the wheel deflected.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_\_\_

4. **Reverse (synthesized scrub)**: get playing again the documented way —
   tap **JOG** (rate 1.0 hand-off), press **STOP/PLAY** (starts at 1×), press
   **SHTL** holding the wheel slightly forward past center. Then sweep the
   wheel to **full center-to-backward** in stages (~¼, ~½, full).

   **Expected:** the playhead moves **backwards** through the item — at ~¼
   backward deflection at ≈ 2× speed, at full deflection at ≈ 8× (stepped
   seeks, not playrate). The sound is a fast scrub: expect discrete jumps /
   stutter, *not* smooth time-stretched audio — that is the implementation,
   not a defect. The playhead never crosses before 0 (clamped). If the
   transport is stopped and you deflect backward instead, **nothing happens**
   (reverse only acts while playing; scrubbing while stopped is JOG's job).

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_\_\_

5. **Hand-offs**: while shuttling (SHTL lit, wheel deflected, playing):
   (a) tap **JOG**; (b) tap **SHTL** to re-enter and tap **STOP/PLAY**.

   **Expected (a):** the rate snaps to **1.0× immediately** and the **JOG**
   LED comes back — the wheel is relative jog again and transport keeps
   playing at normal speed. **Expected (b):** the transport **hard-stops**
   and the mode returns to the one you came from (JOG lit) — STOP/PLAY in
   SHTL is never a play toggle.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_\_\_

6. **Feel — record numbers, not impressions** (answers go to
   `docs/notes/m4-observations.md`). The oracle pins the law's values; only
   you can judge whether they feel right. If something feels wrong, report
   the *parameter* to change and the *number* you want — the law's three
   knobs are all in `ShuttleRate` (`plugin/src/shuttle.cpp`):
   - **`kDeadBand` = 40** (out of 4096, ≈ 1% of travel): too small → you can
     never park without accidentally playing (report, e.g., "want ±80, ≈ 2%");
     too large → the low rates are unreachable ("want ±20").
   - **Max rate = 8.0** (the `7.0` gain in `1 + 7·s²(3−2s)`): the *audible* forward
     top is REAPER's 4.0× cap, which no parameter here can raise; changing
     the gain rescales *where* each rate sits (e.g., "2× at ¼ turn is too
     fast for fine review — want 2× at ~¾ turn, i.e. max ≈ 2.5").
   - **Curve = smoothstep** `s²(3−2s)`: flattens at both ends, so the rate
     barely moves near center and near full deflection. Near-center feel
     ("too sluggish — it takes X% of a turn before it moves") or near-full
     ("the last third of travel does nothing audible") is a curve question
     (e.g., "a linear 1→8 law would put 2× at ~12% of travel").

Pass bar: SHTL LED is truthful (lit exactly in SHTL), deflection continuously
sets the rate with no coast, center is silent and stays silent, reverse
moves the playhead backwards, and both hand-offs (JOG tap, STOP/PLAY exit)
behave in step 5 — with the feel answers in `docs/notes/m4-observations.md`
stating numbers.

## M6 — takes, marker, view

Automated equivalents: `tools/reaper-oracle --layer reaper --scenario simple_verbs` (CAM 2, MARK, FULL VIEW), `--scenario smth_cut` (SMTH CUT), `--scenario place_on_top` (PLACE ON TOP), `--scenario view_solo` (VIDEO_ONLY / AUDIO_ONLY) — these hand tests add real hardware and the audio the oracle cannot judge.

Setup: M1 state (device plugged in, plugin connected). In REAPER: a project with **one item that has three takes** (drop the same file three times into the item's take lanes), **one other item with a single take** on the same track, and a second track with an item on it (for the view buttons). Ripple editing **off**.

1. **CAM 2 / CAM 3 on a three-take item (E6)**: select the three-take item (single click), put the edit cursor inside it. Tap **CAM 2**; listen. Tap **CAM 3**; listen. Then click empty arrange space so **nothing is selected**, move the cursor (JOG) so it is inside the three-take item again, and tap **CAM 1**.

   **Expected:** with the item selected, CAM 2 switches its active take to take 2 and CAM 3 to take 3 — the take lane highlights move and the auditioned/played content is the new take; each tap is one undo step (ESC double-tap goes back one take switch). With **nothing** selected the verb still works: it acts on the item under the edit cursor (the implicit target) — CAM 1 brings take 1 back. The single-take item is **untouched** by any CAM tap (targets with fewer than N takes keep theirs — no error, no change). Undo labels are `Speed Reaper: take 2` / `take 3` / `take 1`. Note: **the CAM LEDs are not yet wired (M8)** — do not expect the CAM 2/CAM 3 panel LEDs to light; the take-lane highlight in REAPER is the truth to check.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

2. **MARK without leaving the device (E8)**: in JOG, turn the wheel to a point you can identify (e.g. a beat), then **double-tap AUDIO LEVEL** (the button's 2nd label is MARK).

   **Expected:** a **project marker is inserted at the edit cursor** (`cmd 40157`, "Markers: Insert marker at current position") — a marker appears on REAPER's marker track exactly at the cursor; the cursor itself does not move. Double-tapping several times places one marker per tap at whatever the cursor currently is. (AUDIO LEVEL taps are deferred ~300 ms like IN/OUT — the double-tap window; a single tap is a volume read-out, next step.)

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

3. **AUDIO LEVEL single tap — volume read-out**: with an item under the cursor (selected or not), tap **AUDIO LEVEL** once.

   **Expected:** after the ~300 ms double-tap window REAPER's console shows `Volume: <x.x> dB` for the first target item (e.g. an item whose volume is at −6.0 dB prints `Volume: -6.0 dB`). No marker is inserted, no selection change, no undo entry — it is a pure read-out.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

4. **SMTH CUT over a jump cut**: set up two items that **meet at a hard boundary** (a jump cut: second item starts exactly where the first ends, same track, audible content difference at the edge). Jog to a position before the boundary; tap **IN**, jog so the cursor is past the boundary, tap **OUT** — a time selection that **contains the boundary**. Tap **SMTH CUT**.

   **Expected:** REAPER's *Crossfade items within time selection* runs (`cmd 40916`): the two meeting items get **auto-fades at the boundary** — a short crossfade appears (fades visible in the item corners in the arrange) and the hard cut plays as a smooth blend. Then clear the time selection (single **ESC** tap) and tap **SMTH CUT** again with **no time selection**: **nothing happens** (console trace `no time selection`) — the verb refuses to guess. Note: the crossfade only appears at item boundaries that lie **strictly inside** the time selection — a selection that starts or ends exactly on the boundary (or spans items with no boundary inside it) produces no fade; widen the selection around the cut.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

5. **PLACE ON TOP**: select one item (single click). Tap **PLACE ON TOP**.

   **Expected:** a **new track is inserted directly above** the item's track and a **copy of the item** lands on it at the **same position** (same length, same take, same source) — the original item stays where it was. The selection goes back to the original item(s), not the copy. It is one undo step: ESC double-tap removes the new track and copy together (`Speed Reaper: place on top`). With nothing under the cursor and nothing selected, the tap is a no-op (trace `target none`).

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

6. **VIDEO_ONLY / AUDIO_ONLY — re-tap restores**: with an item on the first track as the target (selected or under the cursor), make sure no tracks are soloed and all tracks are visible. Tap **VIDEO_ONLY**. Tap it **again**. Then tap **AUDIO_ONLY**; tap it **again**.

   **Expected (VIDEO_ONLY):** first tap — **isolate view**: every track except the target's track is hidden in the arrange (hidden rows collapse/gray out, only the target track remains visible). Second tap — **exactly the previous per-track visibility is restored** (all tracks visible again). **Expected (AUDIO_ONLY):** first tap — the target's track is **soloed** (SOLO lit on that track; everything else silent while playing). Second tap — the **previous solo flags are restored** (solo off again). The saved state lives in plugin memory only: the taps create **no undo entries** — ESC double-tap does not restore the view/solo state, only the re-tap does. (Track-solo/isolate LED mapping on the device is M8.)

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

7. **FULL VIEW**: tap **FULL VIEW**. Look at the REAPER window; tap it again.

   **Expected:** `cmd 40346` is REAPER's **"Toggle fullscreen"** — the **entire REAPER window** goes fullscreen (all panels, menu bar hidden, window fills the screen) and the second tap returns to the normal window. This is **not** "maximize arrange": REAPER 7.82 has **no such action** (`docs/reaper-actions.tsv` has no "maximize arrange" entry — the design doc's candidate name was settled to Toggle fullscreen by the `simple_verbs` oracle scenario). If you want arrange-maximized behavior, that would be a different action to map in a later milestone — record whether fullscreen is a usable substitute for your review workflow.

   ☐ pass ☐ fail — notes: \_\_\_\_\_\_\_\_

Pass bar: CAM 2/3 switch the active take on selected items *and* on the cursor item with nothing selected (fewer-take items untouched, CAM LEDs still off — M8), MARK double-tap drops a project marker at the cursor and a single tap prints the item's volume, SMTH CUT crossfades a boundary that lies inside the time selection and no-ops without one, PLACE ON TOP duplicates the target onto a new track above in one undo step, VIDEO_ONLY/AUDIO_ONLY isolate/solo on tap and fully restore on re-tap (no undo entries), and FULL VIEW toggles REAPER's fullscreen window.
