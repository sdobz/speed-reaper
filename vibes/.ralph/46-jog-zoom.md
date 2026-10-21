# Ralph record: 46-jog-zoom

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- Changing `plugin/src/default_bindings.ini` requires re-running `tools/gen_bindings_doc docs/bindings.md`; `tools/check` fails on a stale `docs/bindings.md`.


## Task: 1. Float `[tuning]` key `wheel.pixels_per_detent`

### Attempt 1 — 2026-10-21T06:26:59Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - Tree was clean; implemented the whole task in one pass.
  - `bindings.cpp`: new `ParsePositiveFloat` (strtod, rejects empty / trailing junk / !isfinite / <=0), checked for `lhs == "wheel.pixels_per_detent"` before the int path in the `[tuning]` section; added `<cmath>` include.
  - `bindings.h`: `double wheel_pixels_per_detent = 0.25;` in `Tuning`.
  - `default_bindings.ini`: commented `;wheel.pixels_per_detent = 0.25` under the existing keys.
  - `bindings_test.cpp::test_tuning`: default 0.25, `0.5`, `2` (int-literal-as-float) accepted; `abc`, `0`, `-1`, `""` each rejected with 1 error and default kept.
  - Plain `bash` has no cmake/ctest in PATH — use `nix develop --command <tool>` or `tools/check` (which self-wraps).
  - Quick pass: `cmake --build build/plugin` clean with -Werror; ctest -R bindings: 3/3 pass (bindings_test, default_bindings_test, user_bindings_test).
- Dead ends:
  - First `tools/check bindings` run failed: docs/bindings.md is regenerated from the default ini's `[tuning]` block — after touching `default_bindings.ini` run `tools/gen_bindings_doc docs/bindings.md` and commit the diff.
- Next:
  - DONE: `tools/check bindings` exited 0 (job `check-bindings`, second run); line 26 ticked.

Result: exit 0; done; log: logs/46-jog-zoom-L26-a1-1792564019590.log

### Done — 2026-10-21 — 1 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```

## Task: 2. Zoom-scale seam + `JogEngine` uses pixels per detent

### Attempt 2 — 2026-10-21T06:30:54Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - Tree was clean (task 1 already committed); implemented the whole task in one pass.
  - Task text inconsistency resolved: the per-tick formula `(delta / kUnitsPerRev) * (k_px / pxPerSec)` would give one revolution = k_px/pxPerSec, contradicting the task's own test spec ("one revolution = k_px * 4096 / pxPerSec") and the doc's design ("one detent moves k_px pixels"). Implemented the detent-based scale: `dt = (delta / kUnitsPerDetent) * (k_px / pxPerSec)` with `kUnitsPerDetent = 360`; `kUnitsPerRev` kept unchanged (1474560).
  - `jog_engine.h`: new `IZoomSource { virtual double PixelsPerSecond() const = 0; }` (doc §4.1 option a). `JogEngine` ctor `(IWheelSource&, IPlayheadSink&, IZoomSource&, double pixels_per_detent = 0.0)`; guard `pxPerSec <= 0` → no move; `SetPixelsPerDetent()` for re-threading after ReloadBindings.
  - `kSecondsPerRev` removed; `kSecondsPerWheelUnit = 20.0 / kUnitsPerRev` KEPT as an interim constant (Router/wheel_dispatcher_test/router_test still use it — task 3 re-points the Router; touching it now is out of scope).
  - `reaper_api.h`: `REAPERAPI_WANT_GetHZoomLevel`. `reaper_sinks.{h,cpp}`: `ReaperZoomSource::PixelsPerSecond() { return GetHZoomLevel(); }`. `main.cpp`: `g_reaper_zoom` global passed into Session.
  - `session.{h,cpp}`: Session ctor gains `IZoomSource &zoom` (before the defaulted bindings_text); engine_ init threads `verbs_.tuning().wheel_pixels_per_detent`; `ConfigureGestures()` calls `engine_.SetPixelsPerDetent` (also covers ReloadBindings).
  - `jog_engine_test.cpp`: fake zoom source; rev = k_px*4096/pxPerSec both directions; screen-space-constant across zoom (pixels per detent unchanged at 50 vs 400 px/s); pxPerSec 0 and negative → no move; SetPixelsPerDetent re-thread.
  - `session_test.cpp` (compile break): added `FakeZoom`, all 6 `Session s(dev, sink, proj, ph)` → `..., ph, zoom)`.
  - `SKIP_WINDOWS=1 tools/check jog_engine` → full PASS incl. oracle host layer.
- Dead ends:
  - (none yet)
- Next:
  - DONE: full `tools/check jog_engine` (incl. Windows static + oracle host) exited 0 (job `check46`); line 41 ticked.

Result: exit 0; done; log: logs/46-jog-zoom-L41-a2-1792564254835.log

### Done — 2026-10-21 — 2 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```
