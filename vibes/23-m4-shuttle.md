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

- [ ] Done

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

- [ ] Done

### 3. Run the spike under the oracle and write the findings

- Add `oracle/scenarios/spike_rate.lua`: for each spike action (2×, 8×, −1×, reset) run it via `Main_OnCommand(NamedCommandLookup(...), 0)`, wait 600 ms, and record in `check` details the `spike …` trace values plus `reaper.Master_GetPlayRate(0)`, `reaper.GetPlayState()` and two cursor samples 300 ms apart. The checks only assert that each action ran (trace line present); the *measurements* are the output.
- Run `tools/reaper-oracle --layer reaper --scenario spike_rate --report-only` and write `docs/notes/m4-shuttle-api.md` from `build/oracle/`: a table (one row per action: requested rate, read-back rate, play state, cursor moved?, reversed?), the answers to: does 8× play? does a negative rate reverse? what is read back? Include a "Limits" paragraph: headless REAPER has no audio device, so audio quality/scrub *sound* is unmeasured (human item only).
- Then 2–3 candidate approaches (e.g. "playrate only", "playrate forward + stepped seek for reverse", "forward-only shuttle for v1") with tradeoffs, grounded in the measured table. Do not implement any approach.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario spike_rate --report-only >/dev/null; test -s docs/notes/m4-shuttle-api.md && grep -q '^| ' docs/notes/m4-shuttle-api.md
```

- [ ] Done

### 4. (human) Pick the shuttle approach

- [!] Done
  BLOCKED: human reads `docs/notes/m4-shuttle-api.md` (measured by the oracle), checks by ear whether the chosen rates sound acceptable on a real audio device, picks an approach, then writes the follow-up tasks here (suggested: wire `SHTL` in `Router` with `AbsolutePosition()` → `ShuttleRate` → chosen API; STOP/PLAY hard stop; JOG-tap handoff; an oracle scenario `shuttle` with a fake-device script that deflects the wheel; SMOKE "M4 — shuttle" + hand test) and replaces this box.
