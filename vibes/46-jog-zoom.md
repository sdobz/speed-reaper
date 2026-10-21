# Milestone 46 — Screen-space jog/scroll and TIMELINE-hold zoom

Today one wheel detent moves a fixed number of seconds, so on-screen motion
changes with zoom. This milestone makes jog and scroll move a fixed number of
**pixels** per detent, and rebinds `TIMELINE`+wheel to zoom around the edit
cursor (replacing `select_step`, which is removed).

## Context
- `docs/jog-discovery.md` — **normative** for every step (formulas §4, config §5, removals §7). Settled: one knob `wheel.pixels_per_detent` (`k_px`, default 0.25, bigger = faster); CW = zoom in; pivot = edit cursor; `adjustZoom` is NOT used.
- Settled here (the doc's two open sub-items): the `[tuning]` parser gets a `strtod` path for float keys; zoom is a view change, so **no** `Undo_Begin/End`.
- `docs/notes/m1-wheel-scale.md` — `kUnitsPerRev` = 1474560 (4096 detents × 360).
- `docs/notes/oracle-scenario-rules.md` — read before any oracle step (rebuild first, defer errors, two-attempt limit).
- Any new REAPER API needs a `REAPERAPI_WANT_<name>` line in `plugin/src/reaper_api.h` (`tools/check_api_want`).

### 1. Float `[tuning]` key `wheel.pixels_per_detent`
- `plugin/src/bindings.{h,cpp}`: add `double wheel_pixels_per_detent = 0.25` to `Tuning`; parse the key with `strtod` (reject empty, trailing junk, `<= 0`, non-finite). Other `[tuning]` keys stay integer-only.
- Add the key (commented default 0.25) to the `[tuning]` block of `plugin/src/default_bindings.ini`.
- Tests in `plugin/test/bindings_test.cpp`: default, `0.5`, `2`, `abc`, `0`, `-1` rejected.
- Do not use the value anywhere yet.

**Verify**
```bash
tools/check bindings
```

- [x] Done

### 2. Zoom-scale seam + `JogEngine` uses pixels per detent
- Add `GetHZoomLevel` to `plugin/src/reaper_api.h` (`REAPERAPI_WANT_`).
- Add a small interface `IZoomSource { virtual double PixelsPerSecond() const = 0; }` (shape per `docs/jog-discovery.md` §4.1 option a); REAPER implementation via `GetHZoomLevel` in `plugin/src/reaper_sinks.cpp`.
- `plugin/src/jog_engine.h`: `JogEngine` takes `IZoomSource&` and `k_px`; per tick `dt = (delta / kUnitsPerRev) * (k_px / pxPerSec)`. Guard `pxPerSec <= 0` → no move. Remove `kSecondsPerRev`; keep `kUnitsPerRev`.
- `plugin/src/session.cpp`: thread `k_px` from `Tuning` and the zoom source into `JogEngine`.
- Update `plugin/test/jog_engine_test.cpp` (one revolution = `k_px * 4096 / pxPerSec`, with a fake zoom source). Fix other compile breaks only.
- Do not touch `Router` yet.

**Verify**
```bash
tools/check jog_engine
```

- [ ] Done

### 3. Scrl uses the same scale
- `plugin/src/router.{h,cpp}`: `Router::OnWheel` scrolls by `delta / kUnitsPerRev * k_px / pxPerSec` seconds, sharing `IZoomSource` and `k_px` with `JogEngine` (wire in `session.cpp`). Guard `pxPerSec <= 0`.
- Update `plugin/test/wheel_dispatcher_test.cpp` (~line 84-103) to the new scale.

**Verify**
```bash
tools/check wheel_dispatcher
```

- [ ] Done

### 4. Zoom math as a pure function
- Add `plugin/src/zoom.{h,cpp}` (register in CMake): `struct ViewRange { double L, R; }` and `ViewRange ZoomAround(ViewRange view, double pivot, double detents, double k_px, double px_per_sec)` per `docs/jog-discovery.md` §4.2: `f = k_px / (px_per_sec * W)` clamped `< 1`; CW (detents > 0): `W' = W (1-f)^d`; CCW: `W' = W (1+f)^(-d)`; keep `q = clamp((P-L)/(R-L), 0, 1)`.
- Add `plugin/test/zoom_test.cpp` (register it): CW narrows, CCW widens; `q` preserved; per-turn factor for 4096 detents ≈ 0.59 / 1.7 at 1920 px, k_px 0.25; pivot outside the view clamps; doubling `k_px` doubles the log-step.
- Do not add the verb yet.

**Verify**
```bash
tools/check zoom
```

- [ ] Done

### 5. Verb `zoom_cursor`, replacing `select_step`
- `IProject`: add `PixelsPerSecond()` and `ViewRange GetView()`; `IActionSink`: add `SetViewSpan(double L, double R)` (`GetSet_ArrangeView2` write + `UpdateArrange`). Implement in `reaper_project.cpp` / `reaper_sinks.cpp`; update test fakes.
- `VerbContext` (`plugin/src/verb_registry.h`): add `px_per_detent`, filled by `VerbRouter` from `bindings_.tuning` (`verbs.cpp`).
- `plugin/src/verb_registry.cpp`: add `ZoomCursorVerb` (turn only; `Step` converts `delta / 360` to detents, calls `ZoomAround` with pivot `project.CursorPos()`; no undo block). Register `zoom_cursor`.
- Delete `SelectStepVerb`, `kSelectStepTicks`, its registration and its tests (`verb_registry_test.cpp`, `verbs_test.cpp` select-step case). Repoint `TIMELINE.turn = zoom_cursor` in `default_bindings.ini`, `bindings_test.cpp`, `default_bindings_test.cpp`.
- Add tests in `plugin/test/zoom_cursor_test.cpp` (register it): CW narrows span around the cursor, CCW widens, no undo calls, arg rejection (no args allowed).

**Verify**
```bash
tools/check && tools/check_api_want
```

- [ ] Done

### 6. Re-baseline the oracle and add a zoom scenario
- `oracle/scenarios/hid_wheel.lua`: re-baseline the thresholds (first-turn rise, max step) to the new formula; set or read the headless `pxPerSec` explicitly.
- Add `oracle/scenarios/hid_zoom.lua` (model: `hid_wheel.lua`; add to `REAPER_SCENARIOS` like it): hold `TIMELINE`, turn CW → view span shrinks and the cursor's screen fraction `q` is unchanged within 1e-3; CCW → span grows.
- Follow `docs/notes/oracle-scenario-rules.md`.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario hid_wheel && tools/reaper-oracle --layer reaper --scenario hid_zoom
```

- [ ] Done

### 7. Docs
- `docs/jog-discovery.md`: change the Status line to implemented, tick off the §8 sub-items per the Context above. Update `docs/design.md` / `docs/bindings.md` only where they mention `select_step` or `kSecondsPerRev`.
- Do not change code.

**Verify**
```bash
! rg -q 'select_step|kSecondsPerRev' plugin src docs/design.md docs/bindings.md
```

- [ ] Done
