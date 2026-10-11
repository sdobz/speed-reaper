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
- **Windows**: not buildable with the mingw cross-toolchain — REAPER
  requires MSVC for extensions (see `docs/reaper-dev-env.md` §3).
