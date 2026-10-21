# Speed Reaper

The Blackmagic Speed Editor as a REAPER controller. The wheel jogs, scrolls
and shuttles the transport; the 34 named keys fire REAPER actions, project
operations and item verbs — with **tap**, **double-tap** and **hold + turn**
gestures, and every key + gesture user-bindable through a plain text file
(`docs/bindings.md` is the generated reference, `docs/design.md` the architecture).

## Install

Build once (or grab a release artifact), then put the plugin in REAPER's
`UserPlugins/` directory and restart REAPER:

| Platform | Artifact | Install to |
|---|---|---|
| Linux (x86-64) | `build/plugin/reaper_speed_reaper.so` | `~/.config/REAPER/UserPlugins/` |
| Windows (x86-64) | `reaper/reaper_speed_reaper.dll` | `%APPDATA%\REAPER\UserPlugins\` |

(macOS: **unsupported — build from source**; the driver layer is portable,
only the platform matrix is Linux + Windows — see `CROSS-PLATFORM.md`.)

Linux only:

- **udev rule (recommended)** so the device opens without root (shipped as
  `share/99-blackmagic-speed-editor.rules`, and in the Linux release zip):
  `sudo tee /etc/udev/rules.d/99-blackmagic-speed-editor.rules`:
  ```
  SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1edb", ATTRS{idProduct}=="da0e", MODE="0666"
  ```
  then `sudo udevadm control --reload && sudo udevadm trigger` (or unplug/replug).
  No `/dev/hidraw*` at all → `sudo modprobe hidraw`.
- **Nothing else may hold the device.** DaVinci Resolve (and any running
  `speedcli`) opens the editor exclusively — close them or the plugin cannot
  open the device.

## First run

On first start the plugin writes `<REAPER resource path>/speed-reaper/bindings.ini`
seeded from the embedded defaults (never overwritten afterwards):

- Linux: `~/.config/REAPER/speed-reaper/bindings.ini`
- Windows: `%APPDATA%\REAPER\speed-reaper\bindings.ini`

Syntax: `KEY.gesture = verb [args]` (plus `[tuning]` knobs: `double_tap_ms`,
`hold_ms`, `hold_ticks_per_step`). See `docs/bindings.md`.

## The five actions

In the REAPER action list, search for **"Speed Reaper"**:

| Action | What it does |
|---|---|
| reload bindings | re-read `bindings.ini` from disk |
| open bindings file | open the file in your default editor |
| show bindings | show the currently active bindings in a console message |
| echo fired bindings (toggle) | while on, every fired binding is echoed to the console — the quickest way to see which key/gesture hit which verb |
| learn (pick an action, then press a device key) | opens the REAPER action picker; pick an action, then press any device key (+ gesture) and the binding is written to `bindings.ini` and reloaded |

## Make it yours

1. **Edit → reload:** change `bindings.ini` (via "open bindings file" or your
   editor) and run "reload bindings".
2. **Learn:** run the "learn" action, pick a REAPER action, press the key you
   want — the file is updated for you.
3. **Echo to discover:** turn on "echo fired bindings" and press keys; each
   echo names the key, gesture and verb so you can find what you want to rebind.

Tuning defaults (300 ms double-tap window, 350 ms hold) live in the
`[tuning]` section of the same file.

## LED meanings

Each main-panel key's LED follows the *binding*: it lights when the
**state read** of the verb bound to that key (highest-priority bound
gesture, Tap → Double → Turn → Press) is true. Unbound keys, or
bindings to one-shot actions, stay dark — so rebinding a key to a
different verb changes (or removes) its LED. With the default
`bindings.ini`, that means:

| LED | Lit when (default binding) |
|---|---|
| SNAP | snap is on (`SNAP.tap = action 1157`) |
| CUT | ripple editing = per-track (`CUT.tap = action 40310`) |
| DIS | ripple editing = all tracks (`DIS.tap = action 40311`) |
| LIVE_OWR | trim-behind is on (`LIVE_OWR.tap = action 41117`) |
| CAM1…CAM9 | active take number (1–9) shared by all selected items (`CAM<n>.tap = take <n>`) |
| VIDEO_ONLY | tracks are isolated (`VIDEO_ONLY.tap = isolate_tracks`) |
| AUDIO_ONLY | solo is on (`AUDIO_ONLY.tap = solo_tracks`) |

## Troubleshooting

Device-layer messages land in the REAPER console (View → Toggle REAPER
console); the HID worker logs them from `HidWheelSource`:

- **`device not found`** — no Speed Editor on the USB bus
  (VID `0x1EDB`, PID `0xDA0E`). Check the udev rule/permissions on Linux
  (`ls -l /dev/hidraw*`), `modprobe hidraw` if no node at all, and make sure
  Resolve or `speedcli` is not holding the device. The plugin retries opening
  every second — plug it in and it picks up.
- **`auth failed`** — the challenge-response handshake timed out or was
  rejected; the plugin closes and retries. Usually another program was
  holding the device part-way through auth, or a flaky USB connection —
  replug and check the cable.
- **`bind unresolved <name>`** — a binding references a REAPER action name
  that doesn't exist (typo or action removed); that binding is a no-op.
  Fix or delete the line in `bindings.ini`, then "reload bindings".

## Build from source

```sh
nix develop                  # toolchain (cmake, ninja, gcc, hidapi, gtk3, …)
tools/check                  # builds speedcli + plugin warning-free (-Werror),
                             # runs unit tests, checks the ReaperPluginEntry
                             # export, regenerates docs/bindings.md, and
                             # cross-builds the Windows DLL
nix build .#reaper-plugin-windows   # Windows .dll cross-build
nix build .#packages.x86_64-linux.speedcli   # speedcli (Speed Editor CLI)
```

`SKIP_WINDOWS=1 tools/check` skips the cross build; `ORACLE_FULL=1` runs the
full oracle suite (`docs/plugin-oracle-design.md`).

## License & credits

- **Vendored Speed Editor driver** — `src/speed_editor/` (BSD-3-Clause,
  Copyright (c) 2026 John Howard): port of the C++ reference driver from
  [John-Howard/PythonSpeedEditor](https://github.com/John-Howard/PythonSpeedEditor)
  (`src/speed_editor/LICENSE`).
- **Vendored driver examples** — `docs/speed-editor-examples/`:
  `python-driver/` and `cpp-driver/`, each BSD-3-Clause, Copyright (c) 2026
  John Howard (their own `LICENSE` files), pinned at upstream commit `28057b4`.
- **REAPER SDK + WDL** — `vendor/reaper-sdk`, `vendor/WDL`: Cockos sources,
  used under the REAPER SDK terms (see `vendor/reaper-sdk/README`).
