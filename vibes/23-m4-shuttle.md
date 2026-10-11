# Milestone 4 — Shuttle (SHTL): law first, REAPER API by spike

Builds on `22-m3-edit-verbs.md`. REAPER has no analog shuttle; we build it
over playrate. The control law is fully specified and testable now; **which
REAPER API makes reverse / 8× playback work is unknown**, so this file stops
at a spike + human checkpoint. Do not guess the API.
Design: `docs/plugin-design-m1-m3.md` §8, `docs/reaper-plugin-workflow.md` §3.
Every Verify starts with `tools/check`.

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
- Each handler: if the transport is stopped start play (`Main_OnCommand(1007, 0)`), call `CSurf_OnPlayRateChange(rate)`, and about 300 ms later (timer callback) print via `ShowConsoleMsg`: the requested rate, `Master_GetPlayRate(nullptr)`, and `GetPlayState()`. For `reverse` also print the playhead position twice (now and 300 ms later) so reversal is visible in the log. Reset restores 1.0 and stops.
- Look up `Master_NormalizePlayRate`, `Master_GetPlayRate`, `CSurf_OnPlayRateChange` in `docs/reaper-api-subset.md` §1/§5 first; do not use other APIs.
- Do not wire the Speed Editor to it. Do not implement shuttle.

**Verify**
```bash
tools/check && strings build/plugin/reaper_speed_reaper.so | grep -q speed_reaper_spike_reverse
```

- [ ] Done

### 3. Write the spike runbook and the options doc skeleton

- Create `docs/notes/m4-shuttle-api.md` with: the question list (does 8× play? does a negative rate reverse? what does `Master_GetPlayRate` read back? is audio scrubbed or muted?), a table with one row per spike action and empty `observed` cells for a human to fill, and a short "candidate approaches" section (2–3 options, e.g. "playrate only", "playrate forward + `CSurf_OnRewFwd`-style stepping for reverse", "forward-only shuttle for v1") with tradeoffs, based on `docs/reaper-api-subset.md`.
- Append a `## M4 — shuttle spike` section to `SMOKE-TEST.md` (numbered steps: run each spike action while a project plays, read the console, fill the table).
- Do not implement any approach.

**Verify**
```bash
test -s docs/notes/m4-shuttle-api.md && grep -q '^## M4 — shuttle spike' SMOKE-TEST.md
```

- [ ] Done

### 4. (human) Run the spike, pick the shuttle approach

- [!] Done
  BLOCKED: human runs the spike, fills `docs/notes/m4-shuttle-api.md`, picks an approach, then writes the follow-up tasks here (suggested: wire `SHTL` in `Router` with `AbsolutePosition()` → `ShuttleRate` → chosen API; STOP/PLAY hard stop; JOG-tap handoff; SMOKE "M4 — shuttle" + hand test) and replaces this box.
