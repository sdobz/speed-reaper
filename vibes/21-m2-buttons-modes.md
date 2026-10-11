# Milestone 2 — Buttons: wheel modes (JOG/SCRL), STOP/PLAY, LEDs

Builds on `20-m1-wheel-playhead.md` (real wheel drives the playhead). Adds
button handling: JOG/SCRL switch the wheel behavior and light their LEDs,
STOP/PLAY toggles transport. Shuttle (SHTL) is deliberately later (M4).
Design: `docs/plugin-design-m1-m3.md` §6. Every Verify starts with `tools/check`.

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
- No new actions; keep demo actions. Do not add other buttons.

**Verify**
```bash
tools/check
```

- [ ] Done

### 4. SMOKE-TEST.md: "M2 — buttons and wheel modes"

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

### 5. (human) Run the M2 hand test

- [!] Done
  BLOCKED: human runs SMOKE-TEST.md "M2". Pass bar: JOG/SCRL/STOP-PLAY work with LEDs truthful; scroll speed acceptable (record in `docs/notes/m2-observations.md`). Then tick this box; next is `22-m3-edit-verbs.md`.
