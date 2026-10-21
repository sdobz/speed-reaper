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

- [x] Done

### 3. Scrl uses the same scale
- `plugin/src/router.{h,cpp}`: `Router::OnWheel` scrolls by `delta / kUnitsPerRev * k_px / pxPerSec` seconds, sharing `IZoomSource` and `k_px` with `JogEngine` (wire in `session.cpp`). Guard `pxPerSec <= 0`.
- Update `plugin/test/wheel_dispatcher_test.cpp` (~line 84-103) to the new scale.

**Verify**
```bash
tools/check wheel_dispatcher
```

- [x] Done

### 4. Zoom math as a pure function
- Add `plugin/src/zoom.{h,cpp}` (register in CMake): `struct ViewRange { double L, R; }` and `ViewRange ZoomAround(ViewRange view, double pivot, double detents, double k_px, double px_per_sec)` per `docs/jog-discovery.md` §4.2: `f = k_px / (px_per_sec * W)` clamped `< 1`; CW (detents > 0): `W' = W (1-f)^d`; CCW: `W' = W (1+f)^(-d)`; keep `q = clamp((P-L)/(R-L), 0, 1)`.
- Add `plugin/test/zoom_test.cpp` (register it): CW narrows, CCW widens; `q` preserved; per-turn factor for 4096 detents ≈ 0.59 / 1.7 at 1920 px, k_px 0.25; pivot outside the view clamps; doubling `k_px` doubles the log-step.
- Do not add the verb yet.

**Verify**
```bash
tools/check zoom
```

- [x] Done

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

- [x] Done

### 6. Re-baseline the oracle and add a zoom scenario
- `oracle/scenarios/hid_wheel.lua`: re-baseline the thresholds (first-turn rise, max step) to the new formula; set or read the headless `pxPerSec` explicitly.
- Add `oracle/scenarios/hid_zoom.lua` (model: `hid_wheel.lua`; add to `REAPER_SCENARIOS` like it): hold `TIMELINE`, turn CW → view span shrinks and the cursor's screen fraction `q` is unchanged within 2 px; CCW → span grows.
- Follow `docs/notes/oracle-scenario-rules.md`.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario hid_wheel && tools/reaper-oracle --layer reaper --scenario hid_zoom
```

- [x] Done
  <!-- ralph: replanned=1 op=AND -->
  - [x] Re-baseline oracle/scenarios/hid_wheel.lua to the pixel scale
    - Touch ONLY oracle/scenarios/hid_wheel.lua (no plugin/env/other-scenario changes; no rebuild — build/plugin .so is current). Read docs/notes/oracle-scenario-rules.md first (do not loosen checks; two-attempt limit, then mark [!]).
    - New scale: seconds = (units/360) * (0.25 / pxPerSec). Read `local pxs = reaper.GetHZoomLevel()` at scenario start (failing check if pxs <= 0) and derive both thresholds from it — do not assume a window size.
    - Turn 1 = 87240 units (137 x 600 + 5040 decay tail; see oracle/fake/wheel_unplug.txt): check rise1 > (87240/360)*(0.25/pxs)/6 (old 0.2 s was ~1/6 of the old 1.18 s expected). Max step: old 0.05 s was ~6x old per-event motion; per-event is now (600/360)*(0.25/pxs) s, so maxstep < 6x that; keep the existing dt>100 ms stall-interval exclusion.
    - Turn 2 (after replug) = 82200 units: check rise2 > (82200/360)*(0.25/pxs)/4.
    - record() the measured pxs and the expected values; rewrite the header comment (still describes the old 20 s/rev scale).
    - Run: tools/reaper-oracle --layer reaper --scenario hid_wheel.
    Verify: `tools/reaper-oracle --layer reaper --scenario hid_wheel`
  - [x] Zoom verb keeps its own ideal view for the hold (no read-back drift)
    - Measured (headless REAPER, pxPerSec 100.6): `GetSet_ArrangeView2` honours writes but snaps each edge to the pixel grid (error up to ~0.012 s = 1 px). One wheel step changes the span by ~1e-4 of itself (< 1 px), so re-reading the snapped view every step pins L and makes `q = (P-L)/W` drift. Real REAPER quantises the same way.
    - `plugin/src/verb_registry.cpp` `ZoomCursorVerb`: in `Begin` read `project.GetView()` once and keep `W` (span) and `q = clamp((P-L)/W, 0, 1)` as members. In `Step`, compute the new span from the kept `W` with the `ZoomAround` math (pass a `ViewRange` built from the kept `W`/`q` and pivot `P`), store the new `W`, and `SetViewSpan(L', R')`. Never read the view back inside `Step`. Reset in `End`. `P` is still `project.CursorPos()` each step.
    - Add a test in `plugin/test/zoom_cursor_test.cpp`: a fake project whose `GetView()` returns a view snapped to 0.01 s on every read; after 100 CW steps `q` of the last written `(L', R')` is within 1e-6 of the `q` at `Begin`, and the span equals the unsnapped `ZoomAround` result.
    - Do not touch `zoom.{h,cpp}`, scenarios or other verbs.
    Verify: `tools/check zoom_cursor`
  - [x] Add the hid_zoom oracle scenario (fixture + env + lua + registration)
    - The files `oracle/fake/hid_zoom.txt`, `oracle/scenarios/hid_zoom.{env,lua}` and the `REAPER_SCENARIOS` entry already exist; only the q check needs fixing. Rebuild first (`tools/check` — the previous step changed the plugin).
    - In `oracle/scenarios/hid_zoom.lua` replace the fixed `qmax - qmin <= 1e-3` with a tolerance from the measured pixel snap: `tol = 2 / (pxs * minspan)` (±1 px on each edge; about 2e-3 at 100 px/s, 9.6 s). Record `tol`. Update the header comment: delete the "Status ... FAILS" paragraph and state the pixel-snap reason. Keep every other check as is.
    - Do not touch the plugin or other scenarios.
    - Run: `tools/reaper-oracle --layer reaper --scenario hid_zoom`.
    Verify: `tools/reaper-oracle --layer reaper --scenario hid_zoom && rg -q 'hid_zoom' tools/reaper-oracle`

### 7. Docs
- `docs/jog-discovery.md`: fix the §4.1 formula to `dt = (delta / 360) * (k_px / pxPerSec)` (one detent = 360 units; the `kUnitsPerRev` form is a typo), and refresh §7 / `docs/design.md` (~line 182) which still describe the 20 s/rev scale. Then change the Status line to implemented, tick off the §8 sub-items per the Context above. Update `docs/design.md` / `docs/bindings.md` only where they mention `select_step` or `kSecondsPerRev`.
- Do not change code.

**Verify**
```bash
! rg -q 'select_step|kSecondsPerRev' plugin src docs/design.md docs/bindings.md
```

- [x] Done
