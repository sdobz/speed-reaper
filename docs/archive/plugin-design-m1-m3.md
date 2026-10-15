# Plugin design for milestones M1–M4 (normative for `vibes/20…23`)

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Settles every name, signature, and file path the M1–M4 tasks use, so no task
has to design. Behavior of the *buttons* stays in
`docs/reaper-plugin-workflow.md` (§ refs below); protocol facts stay in
`docs/speed-editor-capabilities.md`. If a task and this doc disagree, this
doc wins — fix the task.

## §1 Layout and build

```
src/speed_editor/        driver (SpeedEditorHID, key_map.h, event_types.h) — shared with speedcli, do not fork
plugin/src/              plugin code; *pure* logic (no reaper_plugin.h) vs REAPER glue (main.cpp only)
plugin/test/*_test.cpp   unit tests, one ctest each, link PLUGIN_PURE_SOURCES (see plugin/CMakeLists.txt)
tools/check [regex]      build everything -Werror + nm check + ctest; every Verify starts here
```

- New **pure** `.cpp` files go in `PLUGIN_PURE_SOURCES`; only `main.cpp` (and
  `reaper_glue.cpp` if added) may include REAPER headers.
- Tests use `plugin/test/check.h` (`CHECK`, `CHECK_NEAR`, return `check_failures()`).
- **Oracle:** `docs/plugin-oracle-design.md` — every task's Verify that touches plugin runtime behavior uses `tools/reaper-oracle` (headless REAPER + fake device), not a human. New plugin features add `Trace()` lines for anything a scenario must observe (oracle design §2) and a `REAPERAPI_WANT_<fn>` line for every new REAPER function used.
- Linux only for M1–M4 (Windows plugin build is out of scope).

## §2 Threading rule

Only the **REAPER timer callback** (8 ms, main thread) may call any REAPER
API. The HID worker thread never does. Cross-thread data:

| data | direction | mechanism |
|---|---|---|
| wheel delta (relative modes 0/2) | worker → main | `std::atomic<int32_t>` accumulator, `exchange(0)` in `Tick` |
| wheel absolute position (modes 1/3) | worker → main | `std::atomic<int32_t>` latest value |
| button changes | worker → main | mutex + `std::deque<ButtonChange>` |
| log lines | worker → main | mutex + `std::deque<std::string>`; main prints with `ShowConsoleMsg` |
| wheel mode / LEDs | main → worker | mutex-protected "pending output" struct with dirty flags; worker applies it between reads |

## §3 Names (the driver already has a `WheelEvent`)

`src/speed_editor/event_types.h` defines `WheelEvent{mode, delta}`. The
plugin's `jog_engine.h` also has `WheelEvent`; both would meet in one TU. So
the plugin's struct is renamed **`WheelSample{std::int32_t delta}`** (M1 task 1).
Plugin code that includes the driver header uses the driver's `WheelEvent`
only inside `hid_wheel_source.cpp`.

## §4 `HidWheelSource` (`plugin/src/hid_wheel_source.{h,cpp}`)

```cpp
struct ButtonChange { std::uint16_t key; bool down; };   // key = KEY_* from key_map.h

class HidWheelSource : public IWheelSource {
 public:
  HidWheelSource();
  ~HidWheelSource() override;          // calls Stop()
  void Start();                        // spawn worker; idempotent
  void Stop();                         // set flag, join; idempotent; closes device (LEDs off)
  void Tick(WheelSample *out) override;// main thread; delta = accumulated since last Tick
  bool PopButton(ButtonChange *out);   // main thread; false if empty   (used from M2)
  bool PopLog(std::string *out);       // main thread; false if empty
  bool Connected() const;              // atomic
  std::int32_t AbsolutePosition() const; // latest mode 1/3 reading (used from M4)
  void RequestWheelMode(std::uint8_t protocolMode);               // from M2
  void RequestJogLeds(bool jog, bool shtl, bool scrl);            // from M2
  void RequestLeds(std::uint32_t mainMask);  // bit i = LED bit i of key_map.h; from M3
};
```

Worker loop (this exact behavior):
1. Not open → every 1 s try `open()`; sleep in ≤100 ms slices so `Stop()` is prompt.
   Log only on state *change* (not every retry).
2. `authenticate()`; on failure log `auth failed`, `close()`, go to 1.
3. `setWheelMode(0)`; `setJogLeds(true,false,false)`; log `connected (serial …)`.
4. Loop while running: apply pending outputs, then `readEvent(ev, 20)`.
   - Wheel event: mode 0/2 → `delta_ += ev.wheel.delta`; mode 1/3 → `abs_ = ev.wheel.delta`.
   - Button event: diff `ev.button.keycodes` against the previously held set
     (`DiffHeldKeys`, M2) → push `ButtonChange`s.
   - `readEvent` returns false on timeout **and** on disconnect: after a false,
     check `getState() == State::Disconnected` → log `disconnected`, clear the
     held set and `Connected`, go to 1.
5. On `Stop()`: `close()` (clears LEDs), return.

Log strings are fixed (tests/hand tests grep them): `connected (serial %s)`,
`auth failed`, `disconnected`, `device not found` (logged once until it appears).

## §5 Probe tool (`plugin/tools/hid_probe.cpp`, target `hid_probe`)

Standalone executable linking `HidWheelSource` + driver, **no REAPER**.
`hid_probe [--seconds N] [--buttons]`: `Start()`, then every 8 ms
`Tick`/`PopLog`/(`PopButton`), printing log lines and a running wheel sum
once per second; exits 0 after N seconds (default 5) **whether or not a
device was found**. Output prefixes: `log: `, `wheel: sum=<n>`, `button: <key hex> down|up`.

## §6 Buttons, wheel modes, router (M2)

- `plugin/src/button_diff.h`: `void DiffHeldKeys(const std::vector<uint16_t>& prev, const std::vector<uint16_t>& cur, std::vector<ButtonChange>* out);` — releases first (in `prev` order), then presses (in `cur` order).
- `plugin/src/wheel_mode.h`: `enum class WheelMode { Jog, Shtl, Scrl };` `uint8_t ProtocolMode(WheelMode)` → Jog=0, Scrl=2, Shtl=3 (ABSOLUTE_DEADZERO).
- `plugin/src/action_sink.h`: `class IActionSink { public: virtual ~IActionSink()=default; virtual void RunCommand(int cmd)=0; virtual void ScrollBySeconds(double dt)=0; };` REAPER implements it in `main.cpp` with `Main_OnCommand(cmd, 0)` / `GetSet_ArrangeView2` read + shift both times by `dt` + set + `UpdateArrange` (project from `EnumProjects(-1)`; dt: + = right/later, − = left).
- `plugin/src/router.{h,cpp}`: `class Router` — owns the current `WheelMode` and handles the **press-semantics** buttons (JOG, SCRL, SHTL, STOP/PLAY). Unit-testable (no HID, no REAPER):
  `explicit Router(IActionSink&)`; `void OnButton(const ButtonChange&)`;
  `void OnWheel(std::int32_t delta)` (only meaningful when `Mode()==Scrl`: calls `ScrollBySeconds(delta × JogEngine::kSecondsPerWheelUnit)` — the same timeline scale as the jog cursor, so the view travels the same on-screen distance per wheel unit as the cursor at any zoom; other modes and zero deltas are ignored);
  `WheelMode Mode() const`; `bool TakeModeChange(WheelMode* m)` (true once after each change, so `main.cpp` forwards it with `RequestWheelMode(ProtocolMode(m))` + `RequestJogLeds`).
  Press rules: JOG down → `Jog`; SCRL down → `Scrl`; STOP/PLAY down → `RunCommand(40044)`; **SHTL is ignored until M4** (mode unchanged, no LED change). Initial mode `Jog`.
- `class ModeGatedSource : public IWheelSource` (in `router.h`): `ModeGatedSource(IWheelSource &hid, Router &r)`; `Tick` ticks `hid`; if `r.Mode()==Jog` passes the delta through, else calls `r.OnWheel(delta)` and outputs 0. This is the engine's source (so SCRL never moves the playhead).
- On (re)connect (`Connected()` false→true seen in `main.cpp`'s timer) `main.cpp` re-sends the Router's current mode + LEDs.

## §7 Gestures and verbs (M3)

`plugin/src/gesture.{h,cpp}`:

```cpp
enum class GestureKind { Tap, DoubleTap, HoldStart, HoldRotate, HoldEnd };
struct Gesture { std::uint16_t key; GestureKind kind; std::int32_t delta; }; // delta only for HoldRotate
struct GestureConfig { int double_tap_ms = 300; int hold_ms = 350; };
class GestureEngine {
 public:
  explicit GestureEngine(GestureConfig c = {});
  void SetDoubleTapKeys(std::vector<std::uint16_t> keys);   // keys whose tap waits for a 2nd press
  void OnKey(std::uint16_t key, bool down, std::int64_t now_ms);
  void OnWheel(std::int32_t delta, std::int64_t now_ms);
  void Poll(std::int64_t now_ms);                           // call every timer tick
  bool Pop(Gesture *out);
};
```

Rules: a key released before `hold_ms` is a **Tap**. For keys in the
double-tap set the Tap is deferred `double_tap_ms` after release; a second
press inside the window yields exactly one **DoubleTap** (no Tap). Other keys
emit Tap immediately on release. Held ≥ `hold_ms` **or** wheel motion while
held → `HoldStart` once, then `HoldRotate` per wheel delta, `HoldEnd` on
release (no Tap). While any key is held, `OnWheel` deltas go to the gesture
(not to the jog path) — the caller checks `HoldActive()`
(`bool HoldActive() const`).

`plugin/src/verbs.{h,cpp}`: `class VerbRouter { VerbRouter(IActionSink&); void OnGesture(const Gesture&); };`
mapping (key, kind) → REAPER command id via a **single table** at the top of
`verbs.cpp`. Candidate ids (**UNVERIFIED** — confirmed by the M3 hand test via
Actions list → right-click → *Copy selected action command ID*; fixing a wrong
id is a one-line table edit):

| key | gesture | REAPER action | id |
|---|---|---|---|
| SPLIT | Tap | Item: Split items at edit or play cursor (select right) | 40012 |
| RIPL DEL | Tap | Item: Remove items | 40006 |
| IN | Tap | Time selection: Set start point | 40625 |
| OUT | Tap | Time selection: Set end point | 40626 |
| IN / OUT | DoubleTap | Time selection: Remove (unselect) time selection | 40635 |
| TRIM IN | Tap | Item: Trim items left of edit cursor | 40511 |
| TRIM OUT | Tap | Item: Trim items right of edit cursor | 40512 |
| SNAP | Tap | Options: Toggle snapping | 1157 |
| ESC | Tap | Time selection: Remove (unselect) time selection | 40635 |
| ESC | DoubleTap | Edit: Undo | 40029 |

STOP/PLAY and the wheel-mode keys are not gestures (Router, press semantics, §6;
transport id 40044 is there). IN, OUT, and ESC are in the double-tap set.

## §8 Shuttle (M4)

Law per `docs/reaper-plugin-workflow.md` §3: input = absolute offset in
[−4096, 4096]; dead band |offset| < 40 → stop. Pure function
`plugin/src/shuttle.{h,cpp}`: `double ShuttleRate(std::int32_t offset);` returns
signed playback rate (0 in the dead band; ±1.0 at the dead-band edge; ±8.0 at
|offset| ≥ 4096; smoothstep between: `rate = 1 + 7·s²(3−2s)` with
`s = (|offset|−40)/(4096−40)`, clamped to [0,1]; sign = sign of offset).
Which REAPER API applies it (reverse rates) is M4's research task.
