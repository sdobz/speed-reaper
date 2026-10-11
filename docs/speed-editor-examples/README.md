# Speed Editor reference implementations

Vendored references for the Blackmagic Speed Editor HID protocol, collected for
`vibes/05-prototypes.md` task 1. Answers the "python? c++?" question: **both
exist and are stored here** — Python for quick prototyping (task 3 CLI), C++ for
the actual REAPER plugin (task 4).

## What's in here

| Path | Language | What it is | Upstream | Commit | License |
|---|---|---|---|---|---|
| `python-bmd/` | Python 3 | The original community protocol reference: `bmd.py` implements the full HID handshake (auth, input/output reports, jog modes) + two usage examples | [octimot/blackmagic-speededitor](https://github.com/octimot/blackmagic-speededitor) (fork of `andyshingler/blackmagic-misc`; original protocol analysis by @smunaut) | `0919407` (2022-11-11) | none (all rights reserved — reference only, see source URL) |
| `python-driver/` | Python 3 | Modern self-contained driver `davinci_speed_editor.py`: auth, wheel with mode detection, all 34 named keycodes, both LED subsystems, `--scan`/`--demo` modes | [John-Howard/PythonSpeedEditor](https://github.com/John-Howard/PythonSpeedEditor) | `28057b4` (2026-04-27) | BSD-3-Clause (`LICENSE`) |
| `cpp-driver/` | C++17 | **The one to base the REAPER plugin on.** `hid_device.{h,cpp}`: hidapi-based driver with the full challenge-response auth in C++, `readEvent()` event loop, LED control. `key_map.h`: complete keycode table. `main.cpp` + `CMakeLists.txt` are SDR++-specific glue (ignore for REAPER; the driver itself only needs hidapi) | [John-Howard/PythonSpeedEditor](https://github.com/John-Howard/PythonSpeedEditor) `plugin/` | `28057b4` (2026-04-27) | BSD-3-Clause (`LICENSE`) |

Files per dir: `python-bmd/` = `bmd.py` (299 lines, the protocol core),
`speed-editor-demo.py` (minimal usage), `speed-editor-to-mackie.py` (remapping
example), `requirements.txt`. `python-driver/` = `davinci_speed_editor.py`
(556 lines). `cpp-driver/src/` = `hid_device.cpp` (316), `hid_device.h` (58),
`key_map.h` (159), `event_types.h` (33), `main.cpp` (1082, SDR++ glue).

## Protocol cheat sheet (verified against the vendored code)

- **Device:** USB HID, VID `0x1EDB` (Blackmagic), PID `0xDA0E`. Plain `hidraw`
  device on Linux; needs group membership or a udev rule
  (`ATTR{idVendor}=="1edb", ATTR{idProduct}=="da0e", MODE="0666"`).
- **Auth (mandatory, mutual):** challenge-response via HID **SET_FEATURE /
  GET_FEATURE on report ID 6**. Until it succeeds the device sends *no* input
  reports. Public algorithm is `bmd_kbd_auth(challenge)` — see
  `python-bmd/bmd.py:145` (two 8-entry lookup tables + `rol8` rotations + mask)
  and the C++ port in `cpp-driver/src/hid_device.cpp` (`calculateResponse`).
- **Input reports:**
  - `0x03` wheel: `u8 mode` + `le32 signed delta` (relative).
  - `0x04` keys: 6 × `LE16` keycodes — the full held-key set on every change,
    no auto-repeat. Empty set = all released.
  - `0x07` battery level.
- **Output reports:**
  - `0x02` main panel LEDs: 3-byte bitfield, **18 LEDs** (bit positions in
    `bmd.py` `SpeedEditorLed` and `key_map.h` / README of upstream).
  - `0x03` set jog mode: `u8 mode` + `u32 0` + `u8 255`.
  - `0x04` jog-mode button LEDs: 1-byte bitfield (JOG / SHTL / SCRL).
- **Jog modes** (the SHTL / JOG / SCRL buttons switch these):
  - `0` = relative (jog), `1` = absolute continuous (±4096 ≈ half turn,
    baseline captured when mode is set), `2` = relative (same as 0),
  - `3` = absolute with deadzone around zero.
- **Keys:** 34 buttons, keycodes `0x0001`–`0x003C` — full named table in
  `cpp-driver/src/key_map.h` (C++ `constexpr`) and `python-bmd/bmd.py`
  (`SpeedEditorKey` IntEnum). Includes STOP/PLAY `0x3C`, JOG `0x1D`, SHTL
  `0x1C`, SCRL `0x1E`, CAM 1–9 `0x33`–`0x3B`, IN `0x07` / OUT `0x08`, etc.

## Notes for the next tasks (05-prototypes #3 and #4)

- **Task 3 (plain CLI in the nix dev env):** use `python-driver/` (or
  `python-bmd/`) as the reference for `--scan`/`--demo` behavior; the dev shell
  has `hidapi` + `libusb1` + `udev` already. A C++ CLI against
  `cpp-driver/src/hid_device.*` also works out of the box — the driver is
  REAPER-free and only links `hidapi` (`pkg-config --cflags --libs
  hidapi-libusb`).
- **Task 4 (REAPER plugin):** port `hid_device.{h,cpp}` into a worker thread
  inside the extension (never block the REAPER UI thread); map
  `WheelEvent`/`ButtonEvent` to transport actions. `main.cpp`'s event-loop and
  LED-update patterns are the template, but the SDR++ parts go away.
  - The only non-hidapi dependency is `<utils/flog.h>` (SDR++ logging, 10
    call-sites in `hid_device.cpp`, 19 in `main.cpp`). Verified: with a
    throwaway `flog::{info,warn,error}` sink stub the driver passes
    `g++ -std=c++17 -fsyntax-only $(pkg-config --cflags hidapi-libusb)` in the
    nix dev shell. Replace with REAPER's own logging (or nothing) when porting.
- Both Python refs assume the device is **not** claimed by DaVinci Resolve
  (a claimed device won't answer).
- Upstream also has: `sdrpp_bridge.py` (rigctl daemon, skipped — SDR++-only),
  `config.yaml`, and `docs/` plans in John-Howard/PythonSpeedEditor, and
  `speed-editor-to-mackie.py`-style remapping demos in octimot's repo.
