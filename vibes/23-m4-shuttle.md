# Milestone 4 — Shuttle (SHTL): law first, REAPER API by spike

Builds on `22-m3-edit-verbs.md`. REAPER has no analog shuttle; we build it
over playrate. The control law is fully specified and testable now; **which
REAPER API makes reverse / 8× playback work is unknown**, so this file stops
at a spike + human checkpoint. Do not guess the API.
Design: `docs/plugin-design-m1-m3.md` §8, `docs/reaper-plugin-workflow.md` §3.
The spike runs under the oracle (`docs/plugin-oracle-design.md`), not by hand. Every Verify starts with `tools/check`.

### 1. `ShuttleRate` (pure)

- Create `plugin/src/shuttle.h/.cpp` with `double ShuttleRate(std::int32_t offset)` exactly as design §8; add it to `PLUGIN_PURE_SOURCES`.
- Add `plugin/test/shuttle_test.cpp`: offsets 0, ±39 → 0; ±40 → ±1.0; ±4096 and ±5000 → ±8.0; monotonic in |offset| over 40..4096; odd symmetry (`rate(-x) == -rate(x)`); a mid value (offset 2068 → `s≈0.5` → rate 4.5 within 0.01).
- Do not touch `main.cpp`.

**Verify**
```bash
tools/check shuttle
```

- [x] Done

### 2. Spike actions: probe the playrate API

Goal: find out how REAPER behaves, not to build the feature.

- In `plugin/src/main.cpp` register four actions (same pattern as the existing ones): `speed_reaper_spike_rate2` (“Speed Reaper: spike rate 2x”), `speed_reaper_spike_rate8` (8x), `speed_reaper_spike_reverse` (−1×), `speed_reaper_spike_reset` (1×).
- Each handler: if the transport is stopped start play (`Main_OnCommand(1007, 0)`), call `CSurf_OnPlayRateChange(rate)`, and about 300 ms later (timer callback) print via `ShowConsoleMsg` **and** `Trace("spike rate=%f read=%f state=%d pos=%f")`: the requested rate, `Master_GetPlayRate(nullptr)`, and `GetPlayState()`. For `reverse` also print the playhead position twice (now and 300 ms later) so reversal is visible in the log. Reset restores 1.0 and stops.
- Add a `REAPERAPI_WANT_` line for each REAPER function used.
- Look up `Master_NormalizePlayRate`, `Master_GetPlayRate`, `CSurf_OnPlayRateChange` in `docs/reaper-api-subset.md` §1/§5 first; do not use other APIs.
- Do not wire the Speed Editor to it. Do not implement shuttle.

**Verify**
```bash
tools/check && strings build/plugin/reaper_speed_reaper.so | grep -q speed_reaper_spike_reverse
```

- [x] Done

### 3. Run the spike under the oracle and write the findings

- Add a one-off scenario `oracle/scenarios/spike_rate.lua` (since deleted; findings in `docs/notes/m4-shuttle-api.md`): for each spike action (2×, 8×, −1×, reset) run it via `Main_OnCommand(NamedCommandLookup(...), 0)`, wait 600 ms, and record in `check` details the `spike …` trace values plus `reaper.Master_GetPlayRate(0)`, `reaper.GetPlayState()` and two cursor samples 300 ms apart. The checks only assert that each action ran (trace line present); the *measurements* are the output.
- Run `tools/reaper-oracle --layer reaper --scenario spike_rate --report-only` and write `docs/notes/m4-shuttle-api.md` from `build/oracle/`: a table (one row per action: requested rate, read-back rate, play state, cursor moved?, reversed?), the answers to: does 8× play? does a negative rate reverse? what is read back? Include a "Limits" paragraph: headless REAPER has no audio device, so audio quality/scrub *sound* is unmeasured (human item only).
- Then 2–3 candidate approaches (e.g. "playrate only", "playrate forward + stepped seek for reverse", "forward-only shuttle for v1") with tradeoffs, grounded in the measured table. Do not implement any approach.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario spike_rate --report-only >/dev/null; test -s docs/notes/m4-shuttle-api.md && grep -q '^| ' docs/notes/m4-shuttle-api.md
```

- [x] Done

### 4. Decide the shuttle approach by rule (no human)

- Write `docs/plugin-design-m4-shuttle-impl.md` from `docs/notes/m4-shuttle-api.md`. Rule, in order, state which won: (1) if 8× forward and −1× reverse both ran (rate read back, cursor moved in the right direction) → "playrate only"; (2) else if forward works but reverse does not → forward via playrate, reverse via stepped seek (`SetEditCurPos` back by `rate*dt` each timer tick); (3) else forward-only shuttle, reverse offsets clamp to the dead band and a one-time message says so. Fix in the doc: which function sets the rate, tick period, how STOP/PLAY and JOG-tap hand off (design §8 of `plugin-design-m1-m3.md`).
- Do not implement.

**Verify**
```bash
test -s docs/plugin-design-m4-shuttle-impl.md && grep -q 'Decision' docs/plugin-design-m4-shuttle-impl.md
```

- [x] Done

### 5. Wire SHTL per the decision

- In `Router`/`main.cpp`: SHTL mode feeds `AbsolutePosition()` → `ShuttleRate` → the chosen API per the doc; dead band pauses (no rewind); STOP/PLAY hard stop; JOG/SCRL press hands off (rate 1.0, relative mode). Remove the four spike actions' registration only if the doc says to; otherwise keep them. Router unit tests with a fake sink: offset 0 → no rate call; ±2068 → ±4.5; STOP/PLAY while shuttling → stop; JOG press → rate 1.0.

**Verify**
```bash
tools/check router
```

- [x] Done

### 6. Oracle scenario: `shuttle`

- Add `oracle/fake/shuttle.txt` (+ `.env`, `shuttle.lua`): SHTL press, wheel absolute deflection to +4096 → `Master_GetPlayRate` ≈ 8 and cursor advances; back to 0 → paused; −2068 → reverse per the chosen approach (cursor decreases, or the documented clamp); STOP/PLAY → stopped; JOG tap while shuttling → rate 1.0. Add to the default oracle run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario shuttle
```

- [x] Done

### 7. Test instructions: SMOKE-TEST.md "M4 — shuttle"

- Write the hand-test section for what the oracle cannot judge (E2): audio quality at 2×/8×/reverse, the feel of the smoothstep curve near center, dead-band size, wheel parked at center = silent. Per step: action, expected result, pass/fail box; and name the parameters to change (`kDeadBand`, max rate, curve) if the feel is wrong, so the human reports numbers not impressions.

**Verify**
```bash
grep -q '^## M4 — shuttle' SMOKE-TEST.md
```

- [x] Done
