# Ralph record: 46-jog-zoom

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- Changing `plugin/src/default_bindings.ini` requires re-running `tools/gen_bindings_doc docs/bindings.md`; `tools/check` fails on a stale `docs/bindings.md`.
- `GetSet_ArrangeView2` writes snap BOTH edges to the pixel grid (error up to ~0.012 s = 1 px at ~100 px/s; probed: 40 writes of a 1 %-shrinking span centred on 5 s read back within 0.012 s of the request). A per-step shrink is far below 1 px, so re-reading the view each step pins L and drifts q. Never read the view back inside a zoom step; keep the ideal span/q for the hold. Oracle q checks need a ~2 px tolerance.
- One detent = 360 wheel units: `dt = (delta/360) * (k_px/pxPerSec)` (the `kUnitsPerRev` form in docs/jog-discovery.md §4.1 is a typo).


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

## Task: 3. Scrl uses the same scale

### Attempt 3 — 2026-10-21T06:37:19Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: task 2 is merged; JogEngine uses `(delta / kUnitsPerDetent=360) * (k_px / pxPerSec)` (one detent = 360 units per docs/notes/m1-wheel-scale.md; one rev = 4096 detents = k_px·4096 px). The task text's `kUnitsPerRev` denominator is §4.1's typo carried over (task 2's own bullet said the same but the accepted implementation + its acceptance test `rev = k_px·4096/pxPerSec` use kUnitsPerDetent). "Same scale" ⇒ Router uses the identical formula, so the view travels the same on-screen distance per wheel unit as the jog cursor (router.h header promise + §8.1 resolved decision).
- Facts: changed `Router` ctor to `(IActionSink&, WheelState&, IZoomSource&, double k_px=0.0)` + `SetPixelsPerDetent`; `session.cpp` threads `zoom` + `verbs_.tuning().wheel_pixels_per_detent` and re-threads in `ConfigureGestures` (bindings reload). Removed now-unused `JogEngine::kSecondsPerWheelUnit`; updated stale comments in wheel_dispatcher.h / reaper_sinks.h. Tests: wheel_dispatcher_test.cpp + router_test.cpp get a FakeZoom (100 px/s, k_px 0.25) and assert `(45/kUnitsPerDetent)*(kPx/pps)`; added pxPerSec<=0 guard checks in both.
- Next: build (`cmake --build build/plugin`), then `tools/check wheel_dispatcher` via bgrun.
- Done: `tools/check wheel_dispatcher` rc=0 (bgwait `check_wd_full`; job `check_wd` was the SKIP_WINDOWS=1 dry run). wheel_dispatcher_test passed; Windows DLL cross-build + static oracle + host oracle pass. Box ticked, committed.
- Facts for later tasks: `kSecondsPerWheelUnit` is fully gone from plugin/ (only docs/ mentions remain — design.md:182 and jog-discovery.md §7:288-290 still describe the old scale; a docs task may want to refresh those). Oracle scenario `oracle/scenarios/hid_wheel.lua` (§7:291) is NOT yet re-baselined to the px/s-based scale — host oracle passed anyway; watch if a later task touches it.

Result: exit 0; done; log: logs/46-jog-zoom-L52-a3-1792564639486.log

### Done — 2026-10-21 — 3 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```

## Task: 4. Zoom math as a pure function

### Attempt 4 — 2026-10-21T06:44:13Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Tree was clean (task 3 committed); no partial zoom code existed.
  - `plugin/src/zoom.h/.cpp` new: `ViewRange{L,R}` + `ZoomAround(view, pivot, detents, k_px, px_per_sec)` exactly per docs §4.2: `f = k_px/(px_per_sec*W)` clamped to `kMaxF=0.99` (f<1 guard); CW `W*(1-f)^d`, CCW `W*(1+f)^(-d)`; `q = clamp((pivot-L)/W, 0, 1)`; `L' = pivot - q*W'`. Degenerate guards (W<=0, px_per_sec<=0, k_px<=0, detents==0, f NaN) return the span unchanged.
  - `zoom.cpp` added to `PLUGIN_PURE_SOURCES` in plugin/CMakeLists.txt (test glob `*_test.cpp` in test/CMakeLists.txt picks up `zoom_test.cpp` automatically — it also links PLUGIN_PURE_SOURCES, so the src entry covers both).
  - Test: 1920-px setup = 10 s span @ 192 px/s, k_px 0.25 → f≈1.302e-4; per-turn (4096 detents) in ≈ 0.5866 / out ≈ 1.7046, asserted ±0.02/±0.05 around 0.59/1.7 plus exact `(1∓f)^4096` cross-check; q=0.32 preserved; pivot clamps (left→L'=pivot, right→R'=pivot); doubling k_px ⇒ log-step exactly 2× (both directions); f-clamp sanity with k_px = 1920.
- Dead ends:
  - First `tools/check zoom`: 3 test failures, all test-side: (a) log-linearity asserted at 1e-12 — `log(W′/W)=d·log(1∓f)`, so doubling k_px doubles the log-step only up to an O(d·f²)≈1.7e-5 remainder; relaxed to 1e-4. (b) the f-clamp check (k_px = 1920 = full window, f clamped to 0.99) used 10 detents → span 1e-19 s, which vanishes in the L′+W′ rounding at pivot 5 s (R′−L′ = 0); used 2 detents instead.
  - Then `tools/check zoom` rc=1 after all tests passed: the `windows-oracle` static layer failed because the flake packages only git-**tracked** files — `zoom.cpp/h` were untracked, so the Windows CMake configure couldn't find them. `git add` the new files and re-run; rc=0.
- Next:
  - DONE: `tools/check zoom` exited 0 (Windows static + host oracle green); line 64 ticked.

Result: exit 0; done; log: logs/46-jog-zoom-L64-a4-1792565053600.log

### Done — 2026-10-21 — 4 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```

## Task: 5. Verb `zoom_cursor`, replacing `select_step`

### Attempt 5 — 2026-10-21T06:49:41Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - Tree was clean (task 4 committed); all edits done in one pass: project.h (pure `PixelsPerSecond()`/`ViewRange GetView()`, includes zoom.h), action_sink.h (non-pure no-op `SetViewSpan(L,R)` in the RunCommandByName style), reaper_project.{h,cpp} (GetHZoomLevel / GetSet_ArrangeView2 read via EnumProjects(-1)), reaper_sinks.{h,cpp} (SetViewSpan = GetSet_ArrangeView2 write + UpdateArrange + Trace), verb_registry.h (VerbContext += `double px_per_detent = 0.0`), verbs.cpp OnGesture (4th aggregate init from `bindings_.tuning.wheel_pixels_per_detent`), verb_registry.cpp (ZoomCursorVerb: `detents = delta/360`, `ZoomAround(project.GetView(), project.CursorPos(), detents, ctx.px_per_detent, project.PixelsPerSecond())` → `sink.SetViewSpan`; no undo; kSelectStepTicks/StepSelection/SelectStepVerb + registration deleted; `<algorithm>` include dropped).
  - WANT lines for GetHZoomLevel/GetSet_ArrangeView2/EnumProjects/UpdateArrange all pre-existing; only comments updated in reaper_api.h.
  - fake_project.h: new members `pixels_per_second = 100.0`, `ViewRange view{0,10}` + overrides. Other test FakeSinks untouched (SetViewSpan defaults to no-op); new plugin/test/zoom_cursor_test.cpp (auto-globbed by test/CMakeLists.txt) has a recording FakeSink; asserts CW narrows / CCW widens vs ZoomAround cross-check, q=0.32 preserved, no undo calls, args/gesture rejection.
  - default_bindings.ini: TIMELINE.turn = zoom_cursor. verb_registry_test: select_step→zoom_cursor at the two-turn-verbs resolve, Tap-rejection, and bad-args sites; the select_step behavior block + `apply_selection` lambda deleted (sed 390,449d — accidentally ate the first line of the nudge comment, re-added). verbs_test.cpp test_timeline_gestures: cases 6+7 (HoldRotate select steps) deleted (sed 326,378d); cases 1-5 kept. kAllNames deliberately NOT given zoom_cursor (it is turn-only, like select_step was absent).
  - `tools/gen_bindings_doc` uses the PREBUILT build/plugin/verb_list binary — must `cmake --build build/plugin` before regenerating, or the verb table stays stale.
  - Build running as bg job `build46`; then regen docs/bindings.md, git add, run `tools/check && tools/check_api_want`.
- Dead ends:
  - (none yet)
- Next:
  - bgwait build46 → tools/gen_bindings_doc docs/bindings.md → git add -A → DONE: build (bg `build46`, must use `nix develop --command cmake` — plain bash has no cmake) clean with -Werror; `tools/gen_bindings_doc docs/bindings.md` after the rebuild (it uses the prebuilt build/plugin/verb_list binary); `tools/check && tools/check_api_want` rc=0 (bg `verify46`): 24/24 ctest incl. zoom_cursor_test, Windows cross-build, oracle host PASS, 48 REAPERAPI_WANT_ lines OK. Line 78 ticked, committed.

Result: exit 0; done; log: logs/46-jog-zoom-L78-a5-1792565381285.log

### Done — 2026-10-21 — 5 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```

## Task: 6. Re-baseline the oracle and add a zoom scenario

### Attempt 6 — 2026-10-21T07:02:39Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; 1 file changed, 5 insertions(+); log: logs/46-jog-zoom-L90-a6-1792566159365.log

### Replan (depth 0) — SPLIT op=AND → "Re-baseline oracle/scenarios/hid_wheel.lua to the pixel scale", "Add the hid_zoom oracle scenario (fixture + env + lua + registration)"
Trigger: 6 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Why: The task bundles two artifacts (re-baseline hid_wheel.lua + a whole new scenario with fixture/env/lua/suite registration) and the only attempt shows a monologue stall with zero tool use and no progress — over-scoped for one sitting, not a time or design problem.
Rejected: BUDGET: record shows no real progress (stall, not a long build), and budget is already 1800 s. SPIKE: no open design decision — the scale is settled (docs §4.2 + task 2's implementation) and headless pxPerSec is measurable via reaper.GetHZoomLevel(). REWRITE: task text and Verify are correct; only the size is wrong. BLOCK: nothing needs a human.
Log: logs/46-jog-zoom-L90-replan-1792566492421.log

## Task: Re-baseline oracle/scenarios/hid_wheel.lua to the pixel scale

### Attempt 1 — 2026-10-21T07:16:35Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Headless REAPER: reaper.GetHZoomLevel() = 100.0 px/s (measured, not set). Per-event motion = (600/360)*(0.25/100) = 1/240 s; 6x = 0.025 s.
- Fixture totals: turn 1 = 87420 units (task says 87240 — using 87240, check is marginally looser, safe), turn 2 = 82200.
- Engine (plugin/src/jog_engine.h + session.cpp): each 8 ms tick applies the ENTIRE accumulated wheel delta (HidWheelSource::Tick exchanges the backlog); kMaxCatchUpMs=50/kAccCapMs=250 bound tick consumption, NOT the backlog.
- REAPER startup stall (~260 ms in run 1; dialogs dismissed by the oracle harness) is hidden inside start("device") trace-poll: engine timer is stopped while the worker thread keeps accumulating deltas; the first timer callback (trace `timer call=1` at ~295 ms) flushes the whole backlog in one sample interval (0.1208 s = 29 events) with a NORMAL ~32 ms dt, so the existing dt>100 ms exclusion cannot identify it. Flush size is unbounded in stall length.
- Fix in scenario only: exclude + record the first-motion interval (first sample where pos > start pos) as the startup backlog flush; all later intervals still checked < 6x per-event. rise1/rise2/thresholds all per task spec.

Next: re-run tools/reaper-oracle --layer reaper --scenario hid_wheel; if green, tick line 92.
- Done: ORACLE PASS reaper/hid_wheel twice in a row (flush 0.0583 s and 0.0542 s, both excluded+recorded; maxstep 0.0167 < 0.025; rise1 0.6071 > 0.1010; rise2 0.5708 > 0.1427; trace connected/disconnected/connected in order).

Result: exit 0; done; log: logs/46-jog-zoom-L92-a1-1792566995351.log

### Done — 2026-10-21 — 1 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```

## Task: Add the hid_zoom oracle scenario (fixture + env + lua + registration)

### Attempt 1 — 2026-10-21T07:30:11Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); progress; 2 files changed, 6 insertions(+), 1 deletion(-); log: logs/46-jog-zoom-L100-a1-1792567811776.log

### Attempt 2 — 2026-10-21T07:45:36Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts: (attempt 3) All artifacts already in place (fixture/env/lua/registration from attempts 1-2). Run: 5/6 checks PASS; only "cursor screen fraction q unchanged within 1e-3" FAILS: q_first=0.500000, range 0.499697..0.518926 (drift 0.0192 = 19x tolerance; pxPerSec=100, min span 9.6353 s at CW end, max post-CCW 9.9937 s).
Facts: Root cause is NOT the scenario and NOT zoom.h (pure math is q-exact by construction: L'=pivot-q*W'). Plugin trace (build/oracle/trace_hid_zoom.txt, "zoom span=" lines) shows REAPER's GetSet_ArrangeView2 write-back does NOT preserve the left edge headlessly: L pins to a ~0.003-0.012 s grid (0.0031 -> 0.0124 held ~14 writes -> -0.0093 -> -0.0124) while R tracks the span; q=(cursor-L)/W then drifts with the shrinking span. Verified independently from the Lua side in attempt 2 (dbg_drift: set(0.3,10.3)->L 0.297177 etc.; set(0,10) stays exact). Span checks unaffected (pass with margin).
Dead ends: scenario-side tweaks cannot fix it: the task pins cursor=5/view=0-10, L' moves every step (W' changes) so it cannot be pre-aligned to REAPER's snap grid, and the rules forbid loosening the 1e-3 check or touching the plugin. Two honest attempts (this task) are used up per docs/notes/oracle-scenario-rules.md.
Next: marked line 100 [!] BLOCKED with measured values. Needs a decision: glue-side fix (e.g. ReaperActionSink::SetViewSpan compensate the L snap — read back snapped L after write and re-anchor, or a different view-write mechanism) and/or a re-specified q tolerance/setup. Scenario files left in place and registered; probe code removed from hid_zoom.lua, header now documents the failure.

Result: exit 0; worker-blocked; 3 files changed, 18 insertions(+), 2 deletions(-); log: logs/46-jog-zoom-L100-a2-1792568736588.log

### Replan (depth 1) — BLOCK → replan failed (no valid JSON decision block)
Trigger: worker marked it blocked: fixture/env/lua/registration are all in place and 5 of 6 checks pass, but the spec'd "q unchanged within 1e-3" check fails: measured q range 0.499697..0.518926 (drift 0.0192, ~19x tolerance; pxPerSec=100, min span 9.6353 s, max post-CCW 9.9937 s). Root cause is outside this task's scope ("do not touch the plugin"): zoom.h re-anchors on the cursor exactly, but headless REAPER's GetSet_ArrangeView2 does not preserve the written left edge (L pins to a ~0.003-0.012 s snap grid while R tracks the span), so q drifts with the shrinking span. Two honest attempts used per docs/notes/oracle-scenario-rules.md; needs a decision: glue-side L-snap compensation (e.g. in ReaperActionSink::SetViewSpan) and/or a re-specified q tolerance or setup.
Log: logs/46-jog-zoom-L100-replan-1792569409645.log

### Replan (human, depth 1) — BLOCK resolved → split into verb fix + scenario tolerance
Trigger: hid_zoom check "q unchanged within 1e-3" failed (q 0.4997..0.5189).
Why: a human probe showed the cause is pixel snapping of `GetSet_ArrangeView2` writes plus `ZoomCursorVerb` re-reading the snapped view every step (sub-pixel steps never move L, q drifts). Fix is in the verb (keep span/q for the hold), not the scenario; the scenario tolerance becomes 2 px (`2/(pxs*span)`), derived from the measurement.
Rejected: compensating L in `SetViewSpan` (fights the quantiser); loosening to an arbitrary number; skipping the check (pivot preservation in the real host is the point of the scenario).

## Task: Zoom verb keeps its own ideal view for the hold (no read-back drift)

### Attempt 1 — 2026-10-21T14:37:56Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: Attempt 1 (2026-10-21T14:37:56Z) stalled with zero tool use — no plugin changes existed; started clean. `tools/check <regex>` auto re-execs via `nix develop` (no cmake on PATH); `SKIP_WINDOWS=1 tools/check zoom_cursor` passed (build + ctest -R zoom_cursor + oracle host). Existing `test_cw_narrows_ccw_widens` calls Step without Begin and was preserved by keeping a live-read fallback in Step when the hold isn't active; production harness (verbs.cpp:85) always calls Begin for Turn gestures.
- Facts (this attempt): `ZoomCursorVerb` now captures W_ (span) and q_ (clamped cursor fraction) once in `Begin`, builds each Step's input `ViewRange` as `{p - q_*W_, +W_}` from the kept members, chains `ZoomAround`, stores the new W_; never reads `GetView()` in an active Step; `End` resets `active_`. New test `test_hold_ignores_snapped_readback` in zoom_cursor_test.cpp: FakeProject + a test-local `snap()` feeding each GetView() the 0.01 s-snapped last write; after 100 CW steps the last write's q is within 1e-6 of 0.3 and L/R match the unsnapped 100x ZoomAround chain at 1e-9; post-End Step re-reads the live (snapped) view. ctest -R zoom_cursor passed.
- Dead ends: none yet.
- Next: done — full `tools/check zoom_cursor` (incl. Windows cross-build + oracle host) exited 0 (bg job `check-verify`, rc=0); line 100 ticked; committed 8a70438.

Result: exit 0; done; 1 file changed, 1 insertion(+), 1 deletion(-); log: logs/46-jog-zoom-L100-a1-1792593476110.log

### Done — 2026-10-21 — 1 attempt(s)
Files:
```
M vibes/.ralph/46-jog-zoom.md
```
