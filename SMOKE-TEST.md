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

4. **RIPL DEL with and without ripple**: with an item selected, tap **RIPL DEL** with REAPER's ripple editing **off** — undo; re-select the item. Now switch ripple editing on (REAPER: `Alt+P` cycles the ripple/loop modes until the toolbar's ripple icon is active, or Options → Ripple mode), put the cursor between the two items and select one, tap **RIPL DEL** again.

   **Expected:** either way the selected item is deleted (`cmd 40006`, item count drops). With ripple **off** the gap where the item was remains — the neighboring item does not move. With ripple **on** the gap closes — the later item(s) shift left to close it. Turn ripple off again afterwards.

5. **SNAP + LED**: note REAPER's snap state (toolbar). Tap **SNAP** on the device; tap it again; then toggle snap from **REAPER's own toolbar**.

   **Expected:** each device tap toggles REAPER's snap (`cmd 1157`) and the device's **SNAP LED** matches the resulting state (on lit / off unlit) immediately. Toggling snap from REAPER's toolbar updates the LED **within ~1 s** (the plugin polls REAPER's snap state every 250 ms — `led main=<hex>` trace lines, bit 5). The LED is the source of truth to check: it must never disagree with REAPER for longer than a second.

6. **ESC double-tap = one Undo**: set up an undoable change (e.g. split an item, or set a time selection), then **double-tap ESC**.

   **Expected:** exactly one Undo fires (`cmd 40029`, once — not twice) and the change is reverted. Crucially the double-tap does **not** also fire the ESC tap: if a time selection was active before the double-tap, it **survives** the undo (no `cmd 40635` between the two presses) — the undo gesture must not clear the time selection.

7. **Feel of the gesture windows** (record the answers in `docs/notes/m3-observations.md`): the double-tap window is **300 ms** and the hold threshold **350 ms** (`plugin/src/gesture.h`, `GestureParams`). On the real device: can you double-tap IN/OUT/ESC reliably within 300 ms, or do your natural taps land outside it? Does a deliberate press-and-hold (no rotate needed in M3 — hold+rotate is backlog) feel like a hold at 350 ms, or does it fire as a tap? Note whether 300/350 ms feel right as-is or need tuning.

Pass bar: every verb does exactly what the same REAPER action does (split at cursor, time selection set/cleared, trims at cursor, item delete with correct ripple behavior, snap toggled), the SNAP LED tracks REAPER's snap both ways within ~1 s, and ESC double-tap undoes once without clearing the time selection.
