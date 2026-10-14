# Milestone 8 — LED status bar: ripple (3-state), trim-behind, CAM take, isolate/solo

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Builds on `33-m7-hold-rotate.md` and `22-m3-edit-verbs.md` §4 (SNAP LED exists). Makes REAPER's hidden state visible; the device only reads, never writes. Design (normative): `docs/plugin-design-m5-m9.md` §5. Every Verify starts with `tools/check`.

### 1. Research: prove the state reads

- Add `oracle/scenarios/led_reads.lua` (no fake device): set ripple off/per-track/all (`40309`/`40310`/`40311`), trim-behind off/on (toggle `41117` "Options: Trim content behind media items when editing" — find its current state first; `41116` cycles auto-crossfade off/on/trim-behind, `41118`/`41119` enable/disable auto-crossfades; names in `docs/reaper-actions.tsv`), and record `GetToggleCommandState` of `1157`, `40310`, `40311`, `41117` after each change as `check` details. Assert only that each read tracks the change you made; where it does not, find the right query (see `docs/reaper-api-subset.md`, REAPER user guide §7.20) and say so.
- Write `docs/notes/m8-state-reads.md`: a table (state → query → value per setting) from `build/oracle/` output. Fix the candidate ids in design §5 if the measurements differ.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario led_reads && test -s docs/notes/m8-state-reads.md && grep -q '^| ' docs/notes/m8-state-reads.md
```

- [x] Done

### 2. `LedState` + `MainLedMask` (pure)

- Replace `MainLedMask(bool)` in `plugin/src/led_state.h/.cpp` by `MainLedMask(const LedState&)` per design §5 (bit indexes from `key_map.h`, `LED_*`; CAM1..9 bits). Update `led_state_test.cpp`: snap only; ripple 1 → CUT only; ripple 2 → DIS only; trim_behind → LIVE_OWR; `active_take` 2 → CAM2 only, 0 → none; isolate/solo bits; combinations OR together.
- Fix the existing call in `main.cpp` minimally so it compiles (pass a `LedState` with only `snap` set). Do not add reads yet.

**Verify**
```bash
tools/check led_state
```

- [x] Done

### 3. Read the state in the timer loop

- Add `LedState IProject::ReadLedState()` to `IProject`/`ReaperProject`/fake using the queries from `docs/notes/m8-state-reads.md`; `active_take` = the take number shared by all selected items, else 0. `isolate`/`solo` come from the M6 toggle state (add `bool IsolateActive()`, `bool SoloActive()` to `IProject`).
- In `main.cpp`'s 250 ms block call `ReadLedState()`, build the mask, `RequestLeds` only when it changed (keep the reconnect re-send); trace `led main=<hex>` as before.

**Verify**
```bash
tools/check
```

- [x] Done

### 4. Oracle scenarios for the LED bar (4a–4b)

#### 4a. Scenario `leds_ripple`: ripple and trim-behind bits

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules: rebuild first, defer errors, two-attempt limit).
- Add `oracle/fake/leds_ripple.txt` + `.env` + `leds_ripple.lua` (a fake device that taps the needed button and idles ≥ 1.5 s between steps) and the name to `REAPER_SCENARIOS`. Lua drives REAPER state; the scenario asserts the traced `led main=` value after each step, which must appear within 1.5 s.
- Ripple off → no CUT/DIS; `40310` → CUT; `40311` → DIS; trim-behind on → LIVE_OWR.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario leds_ripple
```

- [x] Done

#### 4b. Scenario `leds_cam`: CAM take bit

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules: rebuild first, defer errors, two-attempt limit).
- Add `oracle/fake/leds_cam.txt` + `.env` + `leds_cam.lua` (a fake device that taps the needed button and idles ≥ 1.5 s between steps) and the name to `REAPER_SCENARIOS`. Lua drives REAPER state; the scenario asserts the traced `led main=` value after each step, which must appear within 1.5 s.
- Select a 3-take item with take 2 active → CAM2 bit; two selected items with different active takes → no CAM bit.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario leds_cam
```

- [x] Done

### 5. SMOKE-TEST.md: "M8 — LED bar"

- Add the section: E5 setup (snap + ripple + trim-behind → three LEDs), flip `Alt+P` twice and watch CUT/DIS follow within ~1 s, CAM LEDs follow take changes made in REAPER, isolate/solo LEDs. Expected results per step.

**Verify**
```bash
grep -q '^## M8 — LED' SMOKE-TEST.md
```

- [ ] Done
