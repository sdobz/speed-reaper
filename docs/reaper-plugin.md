# Research: REAPER plugin architecture for the Speed Editor

Scope: how a REAPER extension turns the Blackmagic DaVinci Resolve Speed
Editor into a transport controller — prior art, the SDK surface we will use,
and the end-to-end architecture. Companion to `docs/reaper-dev-env.md`
(toolchain, devShell, HID requirements — §5/§7/§8 there; not repeated here).

## TL;DR

1. **The Speed Editor is a USB HID device, not MIDI**, and it refuses to talk
   until both sides complete a *mutual authentication* handshake (public
   algorithm, report ID 6). There is no OS driver and no REAPER support:
   whoever wants the data (our plugin) must open the device itself,
   authenticate, and poll input reports from its own worker thread.
2. **The protocol is publicly documented by prior art**: `bmd.py`
   (Python/python-hid) and `speed-editor-client` (Go/go-hid). Jog input is a
   signed 32-bit value with a mode byte; keys arrive as the full held-set
   (6×LE16 keycodes, no auto-repeat); we also *write* to the device (LEDs,
   jog-mode selection).
3. **REAPER extensions are simple to register**: one entry point
   (`ReaperPluginEntry`), host APIs via `rec->GetFunc`, and *everything* else
   via `rec->Register(...)`. The hooks we care about are verified present in
   the vendored SDK: `hookcommand2` (intercept any action; carries
   MIDI/OSC/mousewheel values), `timer` (periodic work), `custom_action`
   (our own actions), and `reaper_csurf_reg_t` (full control-surface model).
4. **Two viable architectures**: (A) *minimal* — translate the wheel into
   REAPER's built-in scrub/jog action and let REAPER own the transport
   semantics; (B) *full* — register a control surface
   (`IReaperControlSurface`) so REAPER calls us for jog/shuttle and we stay
   two-way synced with transport state (LED feedback). **Plan: ship A first,
   evolve to B.**
5. **De-risk order matters**: a headless CLI HID reader first (verifies auth
   + reports against real hardware, no REAPER involved), then the minimal
   plugin, then the mode buttons and LED write-back, then the csurf upgrade.

## 1. The device: Speed Editor as a USB HID peripheral

Per `bmd.py` (see §2); **not yet verified against our hardware** — that is
the job of the headless CLI reader (§5).

- USB HID device, VID `0x1edb` / PID `0xda0e`; presents as a keyboard plus
  vendor reports. Appears as `/dev/hidrawN` on Linux (see dev-env doc §7 for
  permissions/stacks).
- **Mandatory mutual auth**: before the device emits any reports, both sides
  exchange SET_FEATURE/GET_FEATURE requests on report ID 6. The algorithm
  (`bmd_kbd_auth`) is public (lookup table + rotation).
- **Input reports** (device → us):

  | ID | Payload | Meaning |
  | --- | --- | --- |
  | `0x03` | u8 mode + signed LE32 value | jog wheel; signed delta or absolute position depending on mode |
  | `0x04` | 6×LE16 keycodes | keys; the **full held set** is sent on every change, no auto-repeat |
  | `0x07` | battery level | housekeeping |

- **Output reports** (us → device):

  | ID | Payload | Meaning |
  | --- | --- | --- |
  | `0x02` | LE32 bitfield | main LED bank |
  | `0x03` | u8 mode, u32 0, u8 255 | set jog mode |
  | `0x04` | u8 bitfield | jog-mode button LED |

- **Jog modes**: `0` = relative, `1` = absolute continuous (±4096 ≈ half a
  turn; baseline is the position when the mode was set), `2` = relative,
  `3` = absolute deadzone.
- **44 keys** incl. STOP_PLAY (`0x3c`), JOG (`0x1d`), CAM1–9, IN/OUT. The
  CAM keys are Resolve-specific; in a DAW they are remappable spares. The
  buttons we care about are the transport cluster (stop/play, shuttle/jog/
  scroll mode selectors).

## 2. Prior art / examples / demos

| Project | What it is | Why it matters |
| --- | --- | --- |
| [`bmd.py` (smunaut/blackmagic-misc)](https://github.com/smunaut/blackmagic-misc/blob/master/bmd.py) — [raw](https://raw.githubusercontent.com/smunaut/blackmagic-misc/master/bmd.py), Python + python-hid, Apache-2.0 | Reference implementation of the Speed Editor HID protocol | Auth handshake, all input/output report layouts, jog modes — the spec we are implementing against |
| [`JamesBalazs/speed-editor-client`](https://github.com/JamesBalazs/speed-editor-client), Go + go-hid | Library + examples | Key table ([`keys/keys.go`](https://raw.githubusercontent.com/JamesBalazs/speed-editor-client/main/keys/keys.go)), jog modes, LED control; [`examples/volume_wheel`](https://raw.githubusercontent.com/JamesBalazs/speed-editor-client/main/examples/volume_wheel/main.go) is a cross-platform "wheel → value" demo |
| [`reaper_csurf`](https://github.com/justinfrankel/reaper-sdk) (in the reaper-sdk repo, `reaper-plugins/reaper_csurf/`) | Official REAPER control-surface plugin source | The pattern for option B (§4): `reaper_csurf_reg_t` → `IReaperControlSurface` |
| [SWS / S&M extension](https://github.com/reaper-oss/sws) | Canonical open-source SDK usage | How real plugins register actions/hooks at scale (full table: dev-env doc §5) |
| REAPER built-in action **"Transport: Scrubbing fine control (MIDI CC relative Only)"** — [reaper.blog, "Scrub and Jog" (2017-12)](https://reaper.blog/2017/12/scrub-and-jog/) | Native transport scrub/jog | Stopped = scrub, playing = jog; response tunable in Preferences → Audio → Playback. Option A (§4) is just: wheel → this action |
| Community "unlock" apps (e.g. via [BMD forum thread](https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=183607)) | Firmware/firmware-ish tools that make the Speed Editor a plain keyboard | The no-plugin alternative; coarse: no mode awareness, no LED feedback — confirms why a plugin is the right path |
| Sibling project `~/projects/speed-audacity` (`README.md`, `plugin.md`) | Same hardware, Audacity side | Concluded Audacity 4.0.0 plugin formats don't fit transport control → the REAPER plugin is the chosen path; the USB/HID access approach can be shared |

Cockos forum thread [`t=250358`](https://forum.cockos.com/showthread.php?t=250358)
discusses this exact device but is bot-walled — skipped.

## 3. REAPER extension mechanics (the SDK surface we use)

All facts verified against `vendor/reaper-sdk/sdk/reaper_plugin.h` and
`reaper_plugin_functions.h` (line numbers in this repo's vendored copy).
Build/install anatomy: dev-env doc §1–§2.

- **Entry**: `ReaperPluginEntry(HINSTANCE, reaper_plugin_info_t *rec)` —
  REAPER calls it once; `rec->GetFunc(name)` fetches host API callbacks,
  `rec->Register(type, ...)` registers everything else. There is no legacy
  `on_main` struct — `Register` is the only path.
- **Registration types we will use** (confirmed present in the header):

  | Type | Header location | What it gives us |
  | --- | --- | --- |
  | `hookcommand2` | `reaper_plugin.h` 213–221 | `bool onAction(KbdSectionInfo *sec, int command, int val, int val2, int relmode, HWND hwnd)` — runs *before every action*, intercepting all sources; `val`/`val2` carry MIDI/OSC/mousewheel values, `relmode` 0/1/2/3. The catch-all if we ever need to hijack an action or inspect wheel/CC values |
  | `hookpostcommand2` | :223 | Post-action hook, 7-arg form with `ReaProject *` — for reacting after REAPER has done the work |
  | `timer` | :247–249 | Periodic `void (*timer_function)()` — coarse-tick work (state sync, LED refresh) |
  | `custom_action` (+ `action_help`) | `REAPER_custom_action_register_t`, ~:1107 | Register our own actions ("Speed Editor: jog cursor", …) that show up in the Actions list and are assignable to keys/controls |
  | `reaper_csurf_reg_t` | :1535–1544 | Register `"csurf"`; `create()` returns an `IReaperControlSurface` — REAPER then models our hardware as a first-class control surface (option B) |

- **Host APIs of note**: `CSurf_ScrubAmt(double amt)`
  (`reaper_plugin_functions.h` ~:880) — scrub the edit cursor by an amount;
  the natural primitive for translating signed jog deltas when we own the
  mapping. Plus the usual transport APIs (`Main_OnCommand` for
  play/stop/record actions, transport state getters) for mode buttons and
  LED feedback.

## 4. Architecture: Speed Editor → REAPER transport

High-level only; component-level design happens in the build tasks.

```
 [USB] Speed Editor
   │  HID reports (0x03 jog / 0x04 keys / 0x07 battery)
   ▼
 L0 Device layer (worker thread, NOT the REAPER UI thread)
    open /dev/hidrawN (hidapi) → mutual auth (report 6) → poll loop
    writes back: LEDs (0x02/0x04), jog mode (0x03)
   ▼
 L1 Device state (lock/atomic): last jog value + mode byte, held key set, battery
   ▼
 L2 Mode interpretation
    SHTL / JOG / SCRL buttons → transport mode (shuttle rate | jog step | scroll)
    + sets device jog mode via output report so the wheel behaves accordingly
   ▼
 L3 REAPER integration (pick one; start with A, evolve to B)
   ▼
 [REAPER transport: cursor / play state]
```

- **L0 — Device layer.** Owns the device handle and the auth handshake,
  polls input reports in a tight loop on a dedicated worker thread, pushes
  decoded events into L1, and performs output-report writes. REAPER's SDK
  gives no device access and no input callbacks, so polling is the pattern
  (dev-env doc §7: never block the UI thread).
- **L1 — Device state.** The decoded, de-duplicated view: jog delta since
  last read (relative modes) or position (absolute modes), the set of held
  keys, mode byte, battery.
- **L2 — Mode interpretation.** The shuttle/jog/scroll buttons select what
  the wheel *means*. Our internal transport mode drives both (a) how we
  translate jog deltas in L3 and (b) which device jog-mode output report we
  send, so hardware behavior and REAPER behavior stay consistent.
- **L3 — REAPER integration, two options:**
  - **Option A — minimal (first milestone).** Translate the wheel directly
    into REAPER's built-in "Transport: Scrubbing fine control (MIDI CC
    relative Only)" action: REAPER itself handles stopped=scrub vs
    playing=jog and its response tuning. Mode buttons trigger ordinary
    transport actions. Implementation is roughly: worker thread → state →
    (timer tick) → trigger action / `CSurf_ScrubAmt`. Fewest moving parts;
    no custom action or surface needed.
  - **Option B — control surface (evolution).** Register `reaper_csurf_reg_t`
    and implement `IReaperControlSurface` (the `reaper_csurf` pattern):
    REAPER calls us with jog/shuttle/scrub requests and we report transport
    state back. Gains: two-way sync (LEDs reflect play state), correct
    jog/scrub semantics via the CSurf API, the device becomes a first-class
    surface in REAPER's model. Cost: a real interface implementation.
  - **Fallback/extension — hooks.** `hookcommand2` is available if we need to
    intercept or retarget actions (e.g. normalize wheel behavior across REAPER
    versions); `hookpostcommand2` + `timer` for post-action UI/LED sync.
- **Data flow discipline.** Device thread → L1 (locked state) → consumed on
  the REAPER side (`timer` tick or csurf callback) → transport action. All
  REAPER API calls happen on the REAPER thread; all HID calls happen on the
  worker thread. No cross-thread calls the other way.

## 5. Build plan / next steps

1. **Headless CLI HID reader** (plain C/C++ + hidapi in the existing devShell):
   auth handshake + dump input reports. Verifies §1 against real hardware
   with zero REAPER involvement (de-risks the biggest unknown first).
2. **Minimal plugin, option A**: entry point + `timer`; wheel → built-in
   scrub/jog action; transport buttons → transport actions.
3. **Mode buttons + write-back**: SHTL/JOG/SCRL → L2 mode, jog-mode output
   report, LED feedback.
4. **Evaluate option B** (csurf) once A is stable; `hookcommand2` only if a
   gap appears.
5. **Windows parity later** (Win32 `hid.dll` pattern per dev-env doc §7).

## Sources

- `bmd.py`: <https://raw.githubusercontent.com/smunaut/blackmagic-misc/master/bmd.py> (repo: <https://github.com/smunaut/blackmagic-misc>, Apache-2.0)
- `speed-editor-client`: <https://github.com/JamesBalazs/speed-editor-client> — `keys/keys.go`, `examples/volume_wheel/main.go`
- REAPER blog, "Scrub and Jog" (2017-12): <https://reaper.blog/2017/12/scrub-and-jog/>
- Cockos forum (bot-walled, skipped): <https://forum.cockos.com/showthread.php?t=250358>
- BMD forum (unlock/keyboard mode): <https://forum.blackmagicdesign.com/viewtopic.php?f=21&t=183607>
- Extensions SDK: <https://www.reaper.fm/sdk/plugin/plugin.php>; mirror <https://github.com/justinfrankel/reaper-sdk> (`reaper-plugins/reaper_csurf/`, `sdk/reaper_plugin.h` lines cited in §3)
- SWS: <https://github.com/reaper-oss/sws>
- Local: `docs/reaper-dev-env.md` (dev env, prior-art table §5, HID requirements §7, verified toolchain §8.1)
