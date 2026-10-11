# Blackmagic Speed Editor — Capabilities

What the Blackmagic DaVinci Resolve Speed Editor can do, for the purpose of
driving REAPER from it. Every fact below is verified against the vendored
reference implementations in `docs/speed-editor-examples/`:

- `python-bmd/bmd.py` — original protocol analysis (smunaut), the protocol core
- `python-driver/davinci_speed_editor.py` — self-contained driver, keycodes "verified from actual hardware testing"
- `cpp-driver/src/key_map.h`, `hid_device.{h,cpp}` — C++17/hidapi port (the one to base the REAPER plugin on)

The two Python references agree exactly on every keycode, LED bit position,
and report layout. Where the two differ in interpretation (e.g. jog-mode
semantics), both readings are noted.

---

## 1. The device

| | |
|---|---|
| **What** | Dedicated editing control surface for DaVinci Resolve: one large jog/shuttle wheel, 43 push buttons (each with an LED-backed or unlit label), two LED subsystems, internal rechargeable battery |
| **USB identity** | Vendor ID `0x1EDB` (Blackmagic Design), Product ID `0xDA0E` — plain USB **HID** device, no drivers needed |
| **Connection** | USB-C (HID); also charges the internal battery over USB-C. Blackmagic's product copy also mentions Bluetooth LE, but all vendored references (and this project) use the USB HID path |
| **Linux access** | Regular `hidraw` node. Needs access to the node, e.g. a udev rule: `ATTR{idVendor}=="1edb", ATTR{idProduct}=="da0e", MODE="0666"` |
| **Exclusivity** | Only one host app can claim the device. If DaVinci Resolve (or another driver) has it claimed, it will not answer us. For our purposes the device must be unplugged from Resolve's hands |

### 1.1 Authentication (mandatory, one-way for us)

The device is **locked until authenticated**: no input reports are sent and LED
output is ignored before a successful challenge-response handshake. The
handshake runs over **SET_FEATURE / GET_FEATURE on report ID `0x06`**, using
10-byte reports `[0x06, <step>, <8 payload bytes>]`:

| Step | Direction | Report | Meaning |
|---|---|---|---|
| 1 | send | `[0x06, 0x00, 8×0x00]` | reset the auth state machine, request challenge |
| 2 | recv | `[0x06, 0x00, u64 LE]` | device challenge |
| 3 | send | `[0x06, 0x01, 8×0x00]` | our challenge (arbitrary; references send zero) |
| 4 | recv | `[0x06, 0x02, u64 LE]` | device's response to our challenge (ignored — we do not verify the device) |
| 5 | send | `[0x06, 0x03, u64 LE]` | our response = `bmd_kbd_auth(challenge)` |
| 6 | recv | `[0x06, 0x04, u16 LE, …]` | status; on success the u16 is a re-auth timeout (observed value `600`, believed seconds — `bmd.py` re-authenticates 10 s before expiry) |

The response algorithm (`bmd_kbd_auth`, reverse-engineered by smunaut,
`python-bmd/bmd.py:145`, identical C++ port in
`cpp-driver/src/hid_device.cpp:39` `calculateResponse`):

```text
n  = challenge & 7
v  = byte-rotate-left(challenge, n)          # rol8n: rotate the 64-bit word by n full bytes
if bit0(v) == bit_n(0x78):  k = AUTH_EVEN[n]
else:  v ^= rol8(v);        k = AUTH_ODD[n]
response = v ^ (rol8(v) & 0xA79A63F585D37BF0) ^ k
```

`AUTH_EVEN` / `AUTH_ODD` are two 8-entry tables of 64-bit constants (full
values in `bmd.py:147-165` and in the C++ header — copy, do not re-derive).

Auth is cheap (a few feature reports) and must be re-run periodically or on
disconnect.

---

## 2. The jog/shuttle wheel

One large wheel with **three user-selectable modes**, switched by the
`SHTL` / `JOG` / `SCRL` buttons — each mode button has its own LED (see §4):

| Mode button | Wheel behaviour (as used by Resolve) |
|---|---|
| **JOG** | Frame-by-frame / relative stepping of the playhead |
| **SHTL** | Shuttle — variable-rate playback, both directions |
| **SCRL** | Scroll the timeline without moving the playhead |

### 2.1 Setting the wheel mode (output)

Output report `0x03` (8 bytes): `[0x03, u8 mode, u32 0x00000000, u8 0xFF]`
(`bmd.py` packs it as `'<BBIB', 3, mode, 0, 255`; `0xFF` is an unknown byte
that works). The protocol defines four modes:

| mode byte | name (`SpeedEditorJogMode`) | semantics |
|---|---|---|
| `0` | `RELATIVE_0` | relative: signed deltas per report |
| `1` | `ABSOLUTE_CONTINUOUS` | absolute position; baseline = wheel position when the mode was set; range `−4096…+4096` ≈ half a turn |
| `2` | `RELATIVE_2` | relative; believed identical to `0` |
| `3` | `ABSOLUTE_DEADZERO` | like `1`, with a small dead band around zero that reports `0` |

### 2.2 Reading wheel motion (input)

Input report `0x03`: `[0x03, u8 mode, i32 LE delta, <unknown tail bytes>]`

- `mode` echoes the wheel's active mode.
- `delta` is a **signed 32-bit** value. In relative modes it is motion since
  the last report; in absolute modes it is the position relative to the
  mode-entry baseline (see §2.1).
- Spin faster → larger magnitudes; direction is the sign.

---

## 3. The buttons — all 43 of them

Buttons are reported in input report `0x04`: `[0x04, 6 × LE16 keycodes]`.

Reporting semantics (important for any consumer):

- The report carries the **full set of keys currently held down**, re-sent
  whenever that set changes. `0x0000` = no key. An all-zero payload = everything released.
- **No auto-repeat** and **no hardware "fast double press" detection** —
  double-tap behaviour must be implemented in software if we ever want it.
- Up to 6 simultaneous keys fit in one report.
- Keycodes are `0x0001…0x003C` with gaps; the gaps
  (`0x12–0x19`, `0x20–0x21`, `0x23–0x24`, `0x27–0x2A`, `0x32`) are unassigned.

> **Note on the count:** `speed-editor-examples/README.md` (task 1) says
> "34 keys". Both vendored drivers enumerate **43** named keycodes and agree
> with each other exactly — 43 is the real count. (The 34 figure is a
> miscount from the vendoring task; the table below is authoritative.)

Keycode table (name = primary label printed on the button; "2nd label" = the
secondary label printed on the same button, from `bmd.py` annotations;
function = what the button does in DaVinci Resolve — context only, the REAPER
mapping is ours to choose in tasks 3–4):

| keycode | name | 2nd label | Resolve function | keycode | name | 2nd label | Resolve function |
|---|---|---|---|---|---|---|---|
| `0x0001` | SMART INSRT | CLIP | smart insert edit | `0x001C` | **SHTL** | | wheel → shuttle mode |
| `0x0002` | APPND | CLIP | append edit | `0x001D` | **JOG** | | wheel → jog mode |
| `0x0003` | RIPL O/WR | | ripple overwrite | `0x001E` | **SCRL** | | wheel → scroll mode |
| `0x0004` | CLOSE UP | YPOS | close up (delete clip, close gap) | `0x001F` | SYNC BIN | | multicam sync bin |
| `0x0005` | PLACE ON TOP | | place clip on top at playhead | `0x0022` | TRANS | TITLE | insert transition |
| `0x0006` | SRC O/WR | | source overwrite | `0x0025` | VIDEO_ONLY | | video-only view/filter |
| `0x0007` | IN | CLR | set/clear in point | `0x0026` | AUDIO_ONLY | | audio-only view/filter |
| `0x0008` | OUT | CLR | set/clear out point | `0x002B` | RIPL DEL | | ripple delete |
| `0x0009` | TRIM IN | | trim clip-in at playhead | `0x002C` | AUDIO LEVEL | MARK | audio level/mark |
| `0x000A` | TRIM OUT | | trim clip-out at playhead | `0x002D` | FULL VIEW | RVW | fullscreen review view |
| `0x000B` | ROLL | SLIDE | roll clip (both edges) | `0x002E` | SNAP | = | toggle snapping |
| `0x000C` | SLIP SRC | | slip source clip | `0x002F` | SPLIT | MOVE | split at playhead |
| `0x000D` | SLIP DEST | | slip destination clip | `0x0030` | LIVE O/WR | RND | live overwrite |
| `0x000E` | TRANS DUR | SET | set transition duration | `0x0031` | ESC | UNDO | escape / undo |
| `0x000F` | CUT | | cut/insert edit at playhead | `0x0033` | CAM 1 | | multicam camera 1 |
| `0x0010` | DIS | | dissolve | `0x0034` | CAM 2 | | multicam camera 2 |
| `0x0011` | SMTH CUT | | smooth cut (auto transition) | `0x0035` | CAM 3 | | multicam camera 3 |
| `0x001A` | SOURCE | | focus source viewer | `0x0036` | CAM 4 | | multicam camera 4 |
| `0x001B` | TIMELINE | | focus timeline viewer | `0x0037` | CAM 5 | | multicam camera 5 |
| | | | | `0x0038` | CAM 6 | | multicam camera 6 |
| | | | | `0x0039` | CAM 7 | | multicam camera 7 |
| | | | | `0x003A` | CAM 8 | | multicam camera 8 |
| | | | | `0x003B` | CAM 9 | | multicam camera 9 |
| | | | | `0x003C` | **STOP/PLAY** | | transport toggle |

Grouped by function (same 43 buttons):

- **Transport (1):** STOP/PLAY
- **Wheel modes (3):** SHTL, JOG, SCRL
- **Multicam (10):** CAM 1–9, SYNC BIN
- **Edit/insert (12):** SMART INSRT, APPND, RIPL O/WR, SRC O/WR, PLACE ON TOP, ROLL, CUT, DIS, SMTH CUT, SPLIT, TRANS, TRANS DUR
- **Trim/points (4):** IN, OUT, TRIM IN, TRIM OUT
- **Clip ops (5):** CLOSE UP, SLIP SRC, SLIP DEST, RIPL DEL, LIVE O/WR
- **View/focus (5):** SOURCE, TIMELINE, VIDEO_ONLY, AUDIO_ONLY, FULL VIEW
- **Toggles/misc (3):** SNAP, AUDIO LEVEL, ESC

---

## 4. LEDs (21 total, two independent subsystems)

LEDs are **set-and-hold state**, not blinky: we write the full desired state
each time. The two subsystems live on **separate output reports** and must be
sent independently.

### 4.1 Main panel LEDs — 18 (output report `0x02`)

`[0x02, b1, b2, b3, b4]` — a LE24 bitfield in bytes 1–3; byte 4 is padding
(`bmd.py` packs `'<BI'`, i.e. ID + 32-bit int). Bit → LED:

| bit | LED | bit | LED | bit | LED |
|---|---|---|---|---|---|
| 0 | CLOSE_UP | 6 | CAM7 | 12 | CAM6 |
| 1 | CUT | 7 | CAM8 | 13 | VIDEO_ONLY |
| 2 | DIS | 8 | CAM9 | 14 | CAM1 |
| 3 | SMTH_CUT | 9 | LIVE_OWR | 15 | CAM2 |
| 4 | TRANS | 10 | CAM4 | 16 | CAM3 |
| 5 | SNAP | 11 | CAM5 | 17 | AUDIO_ONLY |

(Note the CAM LED bits are *not* in camera order — 7, 8, 9 sit at bits
6–8, then 1–3 at bits 14–16. Do not "fix" this to be sequential.)

### 4.2 Jog-mode LEDs — 3 (output report `0x04`)

`[0x04, u8]` — bitfield: bit 0 = JOG, bit 1 = SHTL, bit 2 = SCRL.
Normally exactly one is lit, mirroring the active wheel mode.

### 4.3 Coverage

18 buttons have main-panel LEDs + 3 mode buttons have jog LEDs = 21 lit
buttons; the other 22 buttons (all trim/point/edit buttons) are unlit.
Use the LEDs for state feedback we *do* own (e.g. snap on, active cam,
wheel mode) — the lit set is the interface's status bar.

---

## 5. Battery (input report `0x07`)

`[0x07, u8 charging, u8 level]` — `charging` is 0/1 (USB power present),
`level` is 0–100. Sent on change. Lets the plugin show/act on power state
(e.g. log a warning at low battery, or treat `charging==1` as "safe to
idle").

---

## 6. Complete report reference

| report ID | direction | payload | purpose |
|---|---|---|---|
| `0x02` | **out** (write) | `u8 id, b1..b3 LEDs, u8 pad` | 18 main-panel LEDs (LE24 bitfield) |
| `0x03` | **out** (write) | `u8 id, u8 mode, u32 0, u8 0xFF` | set wheel mode (0–3) |
| `0x04` | **out** (write) | `u8 id, u8 leds` | 3 jog-mode LEDs |
| `0x03` | **in** | `u8 id, u8 mode, i32 LE delta, …` | wheel motion (mode echo + delta) |
| `0x04` | **in** | `u8 id, 6 × LE16 keycodes` | held-button set (full state, no repeat) |
| `0x07` | **in** | `u8 id, u8 charging, u8 level` | battery status |
| `0x06` | **feature** | `u8 id, u8 step, u64 LE` | auth handshake (steps `0x00`/`0x01`/`0x03` out; `0x00`/`0x02`/`0x04` in) |

⚠️ IDs `0x03` and `0x04` are each used for **both** an input and an output
report (different HID report types, same ID). Parse by direction + type, not
ID alone.

---

## 7. Capability summary for the REAPER plugin

What each hardware capability buys us in REAPER (mapping decisions belong to
tasks 3–4; this is the inventory):

| hardware | REAPER candidate |
|---|---|
| wheel, JOG mode (relative) | playhead nudge / scrub at playhead (frame or time-based) |
| wheel, SHTL mode (shuttle) | transport play with variable rate + direction (the "smooth playhead control" the plugin must demo) |
| wheel, SCRL mode | scroll timeline view |
| STOP/PLAY | `Action: Transport play/stop toggle` |
| SNAP | toggle magnet/snapping (`Action: View: Toggle snap` or project magnet) |
| ESC (UNDO) | `Action: Edit undo` |
| IN / OUT / TRIM IN / TRIM OUT | set edit points / trim clip edges at edit cursor |
| SMART INSRT, APPND, RIPL O/WR, SRC O/WR, CUT, SPLIT, RIPL DEL, CLOSE UP, ROLL, SLIP SRC/DEST, TRANS, TRANS DUR, SMTH CUT, DIS, PLACE ON TOP | REAPER's edit-action equivalents (smart insert, append to end, ripple overwrite, split items at cursor, ripple delete, trim, etc.) |
| CAM 1–9 | select/toggle take or source in a multicam-style setup (REAPER: cycle selected take / select media item on other track — mapping is a design choice) |
| SOURCE / TIMELINE | focus: arrange view / media explorer or track panel |
| VIDEO_ONLY / AUDIO_ONLY | solo video/audio or mute groups; hide tracks |
| FULL VIEW | maximize arrange / full-screen |
| 18 + 3 LEDs | state feedback: active wheel mode, snap on/off, selected cam, transport running, etc. |
| battery report | low-battery warning in REAPER log / action |

Practical notes carried from the vendored code (details in
`speed-editor-examples/README.md`):

- The driver must run in a **worker thread** (hidapi read blocks); never on
  the REAPER UI thread.
- The C++ driver's only non-hidapi dependency is an SDR++ logging header
  (`flog`) — stub or drop it when porting; `pkg-config hidapi-libusb` is the
  whole build.
- The dev shell (`flake.nix`) already has `hidapi`, `libusb1`, `udev`.
