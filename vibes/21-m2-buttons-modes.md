# Milestone 2 — Buttons: wheel modes (JOG/SCRL), STOP/PLAY, LEDs

Builds on `20-m1-wheel-playhead.md` (real wheel drives the playhead). Adds
button handling: JOG/SCRL switch the wheel behavior and light their LEDs,
STOP/PLAY toggles transport. Shuttle (SHTL) is deliberately later (M4).
Design: `docs/plugin-design-m1-m3.md` §6. Verified by the oracle (`docs/plugin-oracle-design.md`) before any hand test. Every Verify starts with `tools/check`.

### 1. `DiffHeldKeys` (pure) and use it in the worker

- Create `plugin/src/button_diff.h/.cpp` with `DiffHeldKeys` exactly as design §6 and add it to `PLUGIN_PURE_SOURCES`. `ButtonChange` must move out of `hid_wheel_source.h` into `button_diff.h` (include it from `hid_wheel_source.h`).
- Add `plugin/test/button_diff_test.cpp`: press, release, two keys held then one released, release-all (empty `cur`), no change → no output, ordering (releases first).
- Replace the private diff helper in `hid_wheel_source.cpp` with `DiffHeldKeys`.
- Do not write the Router yet.

**Verify**
```bash
tools/check button_diff
```

- [ ] Done

### 2. `Router` and `ModeGatedSource` (pure)

- Create `plugin/src/wheel_mode.h`, `plugin/src/action_sink.h`, `plugin/src/router.h/.cpp` exactly per design §6 (types, signatures, press rules, `kScrollUnitsPerStep = 20`, SHTL ignored). Add `router.cpp` to `PLUGIN_PURE_SOURCES`. Key codes come from `src/speed_editor/key_map.h` (`KEY_JOG`, `KEY_SCRL`, `KEY_SHTL`, `KEY_STOP_PLAY`).
- Add `plugin/test/router_test.cpp` with a fake `IActionSink`: JOG/SCRL presses change `Mode()` and `TakeModeChange` fires once; releases ignored; SHTL ignored; STOP/PLAY down → `RunCommand(40044)` once; Scrl `OnWheel(45)` → 2 steps and remainder 5 kept; Jog `OnWheel` → no scroll; `ModeGatedSource` passes deltas in Jog and outputs 0 + scrolls in Scrl.
- Do not edit `main.cpp`.

**Verify**
```bash
tools/check router
```

- [ ] Done

### 3. Wire the router into the plugin

- In `plugin/src/main.cpp`: add `ReaperActionSink : IActionSink` (`RunCommand` → `Main_OnCommand(cmd, 0)`, `ScrollView` → `CSurf_OnScroll(steps, 0)`); a global `Router`; the engine source becomes `SummedWheelSource(ModeGatedSource(g_hid, router), g_demo_source)`.
- In `onTimer()` (after the log drain): feed every `g_hid.PopButton()` to `Router::OnButton`; if `TakeModeChange` → `g_hid.RequestWheelMode(ProtocolMode(m))` and `RequestJogLeds` with only the matching LED lit. Track `Connected()` and on a false→true edge re-send the current mode + LEDs (design §6).
- Trace (oracle design §2): `Trace("mode %s")` on every mode change, `Trace("led jog=%x main=%x")` whenever `RequestJogLeds`/`RequestLeds` is called, `Trace("cmd %d")` in `ReaperActionSink::RunCommand`. Add `REAPERAPI_WANT_` lines for new REAPER functions.
- No new actions; keep demo actions. Do not add other buttons.

**Verify**
```bash
ORACLE_FULL=1 tools/check
```

- [ ] Done

### 4. Oracle scenario: `buttons`

- Add `oracle/fake/buttons.txt`: key JOG `0x1D` tap; wheel turn (JOG); key SCRL `0x1E` tap; wheel turn (SCRL); key SHTL `0x1C` tap; key STOP/PLAY `0x3C` tap; `unplug`/`plug`; wheel turn again (must still be SCRL after replug). Timings in `ms`, 300 ms between steps.
- Add `oracle/scenarios/buttons.env` + `buttons.lua`: edit cursor at 10 s. Checks: trace has `mode Jog`→`mode Scrl` in order, no `mode Shtl`; cursor rises during the JOG turn and is **unchanged** (±0.001 s) during the SCRL turn; `led jog=` lines show exactly one LED lit matching each mode; `cmd 40044` appears once and `GetPlayState()` flips; after replug a `led jog=` line for Scrl is traced again.
- Fix plugin bugs the scenario finds (smallest change); do not edit the script timings to hide them. Add to the default oracle run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario buttons
```

- [ ] Done

### 5. SMOKE-TEST.md: "M2 — buttons and wheel modes"

- Append `## M2 — buttons and wheel modes` to `SMOKE-TEST.md` (numbered steps, **Expected** each):
  1. Startup: JOG LED lit, wheel moves the edit cursor (as M1).
  2. SCRL: LED switches to SCRL; turning the wheel scrolls the arrange view **horizontally** and the edit cursor does **not** move; JOG: back to cursor motion. Change mode while the wheel is spinning: LED and behavior flip together.
  3. STOP/PLAY: starts and stops playback; works in JOG and SCRL.
  4. SHTL: pressing it does nothing yet (no LED change) — documented as expected until M4.
  5. Unplug/replug while in SCRL: after reconnect SCRL is still the active mode and its LED is lit.
  6. Tuning note: if scroll feels too fast/slow, edit `kScrollUnitsPerStep` in `router.h` and record the final value in `docs/notes/m2-observations.md`.
- Do not change other sections.

**Verify**
```bash
grep -q '^## M2 — buttons and wheel modes' SMOKE-TEST.md
```

- [ ] Done

### 6. (human) Run the M2 hand test

- [!] Done
  BLOCKED: precondition — `ORACLE_FULL=1 tools/check` green. Human runs SMOKE-TEST.md "M2" (real device only: LED brightness, scroll *feel*). Pass bar: JOG/SCRL/STOP-PLAY work with LEDs truthful; scroll speed acceptable (record in `docs/notes/m2-observations.md`). Then tick this box; next is `22-m3-edit-verbs.md`.
