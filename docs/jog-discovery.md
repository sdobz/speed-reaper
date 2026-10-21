# Jog / timeline-navigation feel — discovery

Status: **implemented** (milestone 46). This doc started as a read-back of a
requested change to the feel of timeline navigation, anchored in the
pre-change implementation (§3 is that pre-46 state); the spec in §4–§6 is
what was built, and every decision point in §8 is resolved.

Terminology revision: the vague term "window width" is **removed**. The wheel
model is expressed in screen space (§2): jog and scroll move a fixed
*pixel* amount per detent; the TIMELINE-hold (CW = in) zooms the view span
around the edit cursor, at a step derived from the same
`wheel.pixels_per_detent` config.

## 1. The request (as understood)

Two changes to how the wheel moves the playhead and how zoom is driven:

1. **Jog and scroll are screen-space-constant.** The wheel's effect is a fixed
   **number of screen pixels per detent**, not a fixed number of seconds. At a
   wider view span (lower zoom) or a wider window there are more seconds per
   pixel, so the same detent covers more absolute time — but the on-screen
   motion is identical. The config scale is **pixels per detent**
   (`wheel.pixels_per_detent`), shared by jog and scroll. This holds under
   both zoom changes and REAPER window resizes (REAPER keeps its
   pixels/second zoom level across resizes; the view span scales with the
   window).
2. **`TIMELINE`-hold + wheel = zoom around the edit cursor** (the *zoom
   modifier* hold). Holding the `TIMELINE` key and turning the wheel changes
   the **view span** centered on the **zoom pivot** = the **edit cursor**, so
   the cursor stays put on screen while the zoom changes. **CW (positive
   wheel delta) = zoom in.** The zoom granularity is derived from the *same*
   `wheel.pixels_per_detent` config: one zoom detent changes the span by the
   same relative fraction as one jog detent traverses the view — doubling the
   config doubles the zoom step (§4.2). This displaces the current
   `TIMELINE.turn = select_step` row; `select_step` is removed (a new
   selection method replaces the feature). This enables a
   macro-navigation loop: hold `TIMELINE` → turn to zoom *out* → release →
   turn a little (each turn still moves the same pixels on screen, which at
   the wider span covers a long time) → hold `TIMELINE` → turn to zoom *back
   in* → fine-jog at the new scale.

## 2. Terminology

The arrange view has a **fixed pixel width** but a **variable view span**.
"Window width" is dropped because it did not say which of these it meant.

### 2.1 The three state variables

| State variable | Symbol | Meaning | Moved by |
|---|---|---|---|
| **cursor** | `P` | edit cursor / playhead position | **jog** |
| **view origin** | `t0` | start time at the left edge of the arrange view | **scroll** (pan) |
| **view span** | `W = t1 − t0` | how much *time* is visible | **zoom** |

**Zoom** is the horizontal scale, `pxPerSec` (pixels/second) =
`pixelWidth / W`. It is *derived*, not an independent state variable —
narrowing the view span **is** zooming in.

### 2.2 The three wheel gestures

Each gesture touches exactly **one** state variable:

| Gesture | Input | Per-detent rule | Writes to |
|---|---|---|---|
| **Jog** | bare wheel | `k_px / pxPerSec` seconds | cursor `P` |
| **Scroll / pan** | `Scrl` + wheel | `k_px / pxPerSec` seconds | view origin `t0, t1` (span constant) |
| **Zoom** | `TIMELINE`-hold (zoom modifier) + wheel | span fraction `f = k_px / (pxPerSec · W)` per detent, pivot fixed; CW = zoom in | view span `W` (origin recomputed) |

- `k_px` = `wheel.pixels_per_detent` (config). Bigger = faster. It is the
  *single* sensitivity knob: jog/scroll pixels per detent **and** the zoom
  span fraction `f` both derive from it (doubling `k_px` doubles both).
- **Zoom pivot**: the timeline point that stays fixed *on screen* during a
  zoom; here the edit cursor `P` (i.e. `q = (P − t0)/W` is preserved).
- Jog and scroll are the *same motion in screen space* (identical relative
  cursor↔content movement); only which variable changes differs. After a
  scroll the cursor's time value is unchanged; after a jog the view's is.
- The invariant: **pixels per detent are constant** under zoom changes and
  under window resizes.

## 3. Current implementation (anchors)

### 3.1 Wheel → playhead (jog)

- `plugin/src/jog_engine.h:37-45` — `JogEngine::Tick()` reads a `WheelSample`
  and moves the cursor by a **fixed** amount:
  `sink_.ScrubBySeconds(ev.delta * kSecondsPerWheelUnit)`, where
  `kSecondsPerWheelUnit = kSecondsPerRev / kUnitsPerRev`,
  `kUnitsPerRev = 1474560.0` (measured hardware constant, 4096 detents × 360,
  see `docs/notes/m1-wheel-scale.md`) and `kSecondsPerRev = 20.0` (a
  **compile-time constant**). So one revolution = 20 s of timeline, *regardless
  of zoom*. The m1 note lists `jog.seconds_per_rev` as a "candidate user
  parameter" that was never wired into `[tuning]`.
- `plugin/src/wheel_dispatcher.h:48-67` — `WheelDispatcher::Tick()` routes the
  wheel: turn-hold → `GestureEngine::OnWheel`; else by mode — **Jog** passes the
  delta through to `JogEngine`, **Scrl** → `Router::OnWheel`, **Shtl** →
  `Router::OnShuttleOffset`.
- `plugin/src/reaper_sinks.cpp:15-19` — `ReaperPlayheadSink::ScrubBySeconds` is
  the primitive: `SetEditCurPos(pos + dt)`; position read via
  `GetPlayPosition` (playing) / `GetCursorPosition` (stopped).

### 3.2 Scrl (view scroll) reuses the same fixed scale

- `plugin/src/router.cpp:11-15` — `Router::OnWheel` scrolls the view by
  `delta * JogEngine::kSecondsPerWheelUnit` (the *same* constant as jog). Its
  header comment (`plugin/src/router.h`) promises "the view travels the same
  on-screen distance per wheel unit at any zoom." **Today that promise is
  violated:** a fixed number of seconds per detent maps to `seconds ×
  pxPerSec` pixels, which changes with zoom. Change 1 below is what actually
  implements the promise.

### 3.3 The only place the arrange-view width is touched

- `plugin/src/reaper_sinks.cpp:36-44` — `ReaperActionSink::ScrollBySeconds`
  calls `GetSet_ArrangeView2(proj, false, 0, 0, &t0, &t1)` to read the
  full-view start/end time, shifts **both** by the same dt (constant span →
  pure pan), writes them back, `UpdateArrange()`. This is the existing seam
  from which the **view span** can be read: `W = t1 − t0`.
- There is **no zoom (px/sec) or span query anywhere** in the plugin today.

### 3.4 Timeline-hold + wheel (turn)

- `src/speed_editor/key_map.h:31` — `KEY_TIMELINE = 0x001B`.
- `plugin/src/default_bindings.ini` — `TIMELINE` binds
  `tap = select_cursor_item`, `double = action 40289`, and
  **`TIMELINE.turn = select_step`** (the "last CompositeRow: TIMELINE
  hold+rotate"). This row is replaced by the zoom gesture
  (`TIMELINE.turn = zoom_cursor`); `select_step` itself is removed (a new
  selection method replaces the feature).
- Turn mechanism (already general):
  - any key with a `.turn` row is a "turn key" — `VerbRouter::TurnKeys()`
    ← `bindings_.turn_keys` (`plugin/src/verbs.h`, `plugin/src/verbs.cpp:16-25`).
  - while a turn key is held, `WheelDispatcher` routes the wheel to
    `GestureEngine::OnWheel`, which emits `HoldStart` then `HoldRotate{delta}`
    (`plugin/src/gesture.h`, `plugin/src/gesture.cpp` `OnWheel`).
  - `VerbRouter::OnGesture` (`plugin/src/verbs.cpp:68-93`) maps
    `HoldStart → verb->Begin(ctx)`, `HoldRotate → verb->Step(ctx, delta)`,
    `HoldEnd → verb->End(ctx)`.
  - a turn verb receives `VerbContext{ IActionSink&, IProject&,
    ticks_per_step }` (`plugin/src/verb_registry.h:33-37`) — so it reaches
    REAPER via **IActionSink** and **IProject** (but **not** `IPlayheadSink`).
- The two existing turn verbs show the exact pattern:
  - `SelectStepVerb` (`plugin/src/verb_registry.cpp:357-369`) — pure `Step`
    accumulator, `kSelectStepTicks` (20) ticks per selection step.
  - `NudgeVerb` (`plugin/src/verb_registry.cpp:443-535`) — `Begin` (resolve
    targets + `project.Undo_Begin()`), `Step` (accumulate ticks via `HoldNudge`,
    apply per step), `End` (`project.Undo_End(label)`). **One hold = one undo
    block.**

### 3.5 Edit cursor (the zoom pivot)

- "Edit cursor position" = `GetCursorPosition()` (SDK: "edit cursor position"),
  already exposed as `IProject::CursorPos()` (`plugin/src/reaper_project.cpp:101`)
  and used by `ScrubBySeconds`.

### 3.6 Zoom APIs

- `plugin/src/reaper_api.h` — **no** zoom `REAPERAPI_WANT_` lines. Present and
  already wired: `GetSet_ArrangeView2`, `GetCursorPosition`, `UpdateArrange`.
  Available in the SDK but **not** wired: `GetHZoomLevel` (pixels/second) —
  now **required** by Change 1 (one new `REAPERAPI_WANT_` line); and
  `adjustZoom(amt, forceset, doupd, centermode)` — its vendored doc only says
  "centermode=-1 for default"; the specific centermode value for "center on
  edit cursor" is unconfirmed, so `adjustZoom` is a risky one-call
  alternative. Avoid.
- Rule to remember: any REAPER API a new code path calls needs a
  `REAPERAPI_WANT_<name>` line here, or the oracle host refuses to load
  (enforced by `tools/check_api_want`).

### 3.7 Config seam

- `[tuning]` is parsed in `plugin/src/bindings.cpp:222-257` into
  `Tuning{ double_tap_ms, hold_ms, hold_ticks_per_step }`
  (`plugin/src/bindings.h:35-39`). **All integer**: the parser rejects any
  non-integer value (`bindings.cpp:247`). Both new config values are floats,
  so a small parser extension is required (or store as scaled integers).

## 4. The spec (restated, anchored)

### 4.1 Change 1 — screen-space-constant jog and scroll

- Read the live zoom scale: `pxPerSec = GetHZoomLevel()` (new `REAPERAPI_WANT_`
  line; exposed through a seam, see below).
- New config `k_px` = `wheel.pixels_per_detent` (default **0.25**):
  **bigger = faster**. One detent moves `k_px` screen pixels; one full turn
  (4096 detents) moves `k_px · 4096` pixels ≈ 1024 px ≈ half a 1920-px screen.
- Per tick (jog):
  ```
  dt = (delta / 360) * (k_px / pxPerSec)
  ```
  replacing `kSecondsPerWheelUnit`. One detent = 360 units
  (`JogEngine::kUnitsPerDetent`, measured); a full revolution is 4096
  detents = 1,474,560 units (`kUnitsPerRev`, kept as the hardware constant).
- **Screen-space-constant** falls out directly: pixels per detent =
  `dt · pxPerSec = k_px`, independent of zoom. It also survives window
  resizes: REAPER keeps `pxPerSec` across resizes while `W` scales with the
  window, so reading `pxPerSec` (rather than deriving from `W`) is what makes
  the motion invariant in both cases.
- **Scroll consequence:** `Router::OnWheel` uses the **same** `k_px / pxPerSec`
  scale to shift `(t0, t1)` — implementing the header promise in §3.2.
- Seam: `JogEngine` must gain a zoom-scale query and take `k_px`:
  - add `double PixelsPerSecond() const` to a seam `JogEngine` can call. Two
    options: (a) a new tiny interface injected into `JogEngine` (keeps it pure
    and REAPER-free, matching its current `IWheelSource`/`IPlayheadSink`
    shape); (b) a method on `IPlayheadSink`. Both implemented in `main.cpp`/
    the REAPER glue via `GetHZoomLevel`.
  - `k_px` comes from `[tuning]` and is threaded into `JogEngine` by `Session`
    (`plugin/src/session.cpp` ctor builds `engine_(dispatcher_, sink)`).
  - `Router::OnWheel` needs the same scale — thread it via `Router`'s existing
    dependencies (or share the seam object with `JogEngine`).

### 4.2 Change 2 — `TIMELINE`-hold + wheel = zoom with pivot = edit cursor

- Replace the `TIMELINE.turn` handler (`SelectStepVerb`) with a new turn verb
  `zoom_cursor`. **CW (positive `delta`) = zoom in** (the m1 notes measure a
  CW rev as a positive delta). `select_step` is removed entirely —
  `SelectStepVerb`, its registration, and its tests go away (a new selection
  method replaces the feature; §7 lists the removals).
- **Granularity is derived from `wheel.pixels_per_detent`** (`k_px`) — there
  is no separate zoom config. One jog detent traverses `k_px` screen pixels
  = a fraction of the view span
  ```
  f = k_px / (pxPerSec · W)          # span fraction per detent
  ```
  A zoom detent changes the span by that same fraction, around the **pivot**
  `P = GetCursorPosition()` (edit cursor; `IProject::CursorPos()`),
  preserving `q = (P − t0)/W`. With `Δd` = signed detents
  (`Δd = delta / 360`; the raw `HoldRotate` delta is in units —
  `gesture.cpp:97` passes the device delta through unconverted):
  - CW (Δd > 0): `W′ = W · (1 − f)^Δd`
  - CCW (Δd < 0): `W′ = W · (1 + f)^(−Δd)`
  Then re-anchor the span at the pivot:
  1. read `(L, R)` = `GetSet_ArrangeView2(0, 0)`;
  2. `q = clamp((P − L) / (R − L), 0, 1)`;
  3. `L′ = P − q · W′`; `R′ = L′ + W′`;
  4. `GetSet_ArrangeView2(0, 0) = (L′, R′)` + `UpdateArrange()`.
  Guard: clamp `f < 1` (degenerate configs only — at sane `k_px`,
  `pxPerSec · W` is the window pixel width ≫ `k_px`).
- **Feel at default** (`k_px = 0.25`, 1920-px window): `f ≈ 0.25/1920` per
  detent; one full turn (4096 detents) multiplies the span by
  `(1−f)^4096 ≈ 0.59` (in) or `(1+f)^4096 ≈ 1.7` (out) — roughly half / double
  per turn, the same order as the jog's ~half-screen per turn. Doubling
  `k_px` doubles `f` and hence the per-turn zoom step.
- Setting a **narrower** (start,end) = zoom in (REAPER fits the fixed-pixel
  window); wider = zoom out. Uses only already-wired APIs plus the
  `GetHZoomLevel` read. `adjustZoom(amt, 0, true, centermode)` is the
  one-call alternative but hinges on an unconfirmed centermode value — avoid.
- Seam: the verb needs `k_px` (new `VerbContext` field, threaded from
  `Tuning` by `VerbRouter` — `verbs.cpp:69` already builds the context from
  `bindings_.tuning`) and two new `IProject` reads: `PixelsPerSecond()`
  (`GetHZoomLevel`) and `ViewSpanSeconds()` (`GetSet_ArrangeView2`). The span
  write goes through a new `IActionSink` primitive (e.g.
  `SetViewSpan(L′, R′)`), implemented in `ReaperActionSink` via
  `GetSet_ArrangeView2` + `UpdateArrange`.
- Zoom is a **view change, not an edit** → **no** `Undo_Begin/End` (unlike
  nudge). ← *confirm (§8, #4).*
- This realizes the macro-navigation loop in §1: pixels per detent are
  constant, so at a wider span the same short turn covers a long time; zoom
  back in and the same short turn becomes a fine jog.

## 5. Config

Add to `[tuning]`:
- `wheel.pixels_per_detent` — default **0.25** (bigger = faster). The
  **single** sensitivity knob: drives jog, scroll, *and* the zoom granularity
  (`f` in §4.2) — doubling it doubles all three. There is no separate zoom
  config.

**Caveat:** the `[tuning]` parser is integer-only
(`bindings.cpp:244-257`, `bindings.h:35-39`). Both values are floats, so a
parser extension is required (a `strtod` path for these keys) or
scaled-integer storage. ← *decision (§8, #3).*

## 6. REAPER API surface

- **Required new:** `GetHZoomLevel` (pixels/second) — one new
  `REAPERAPI_WANT_` line in `plugin/src/reaper_api.h`, plus the
  `tools/check_api_want` / oracle-host re-baseline.
- **Already wired (used by the zoom gesture and the scroll path):**
  `GetSet_ArrangeView2`, `GetCursorPosition`, `UpdateArrange`.
- **Still avoided:** `adjustZoom(amt, forceset, doupd, centermode)` —
  one-call "zoom around a point", but the centermode value for "center on
  edit cursor" is unconfirmed.

## 7. Tests / oracle — re-baselined (done in milestone 46)

- `plugin/test/jog_engine_test.cpp` — one revolution =
  `k_px · 4096 / pxPerSec` with a fake `IZoomSource`; the old
  `kSecondsPerRev` (20 s/rev) assertion and the constant itself are gone.
- `plugin/test/wheel_dispatcher_test.cpp` — Scrl scrolls the same
  `k_px / pxPerSec` scale (the old fixed `45.0 * kSecondsPerWheelUnit`
  assertion is gone).
- `oracle/scenarios/hid_wheel.lua` — re-baselined off the measured headless
  `GetHZoomLevel()`: the rise/stall thresholds derive from
  `(units / 360) * (0.25 / pxPerSec)` at the scenario's `pxPerSec` instead
  of the old 20 s/rev constants.
- **Removed with `select_step` (done):** `plugin/test/verb_registry_test.cpp`
  (build/gesture/arg-rejection + the hold-step case),
  `plugin/test/verbs_test.cpp` (HoldRotate select-step case),
  `plugin/test/bindings_test.cpp` and
  `plugin/test/default_bindings_test.cpp` (the `TIMELINE.turn` row —
  repointed at `zoom_cursor`).
- **New coverage (added):** `plugin/test/zoom_test.cpp` — the zoom math: `f`
  derivation, per-turn factor `(1∓f)^4096`, pivot preservation (`q`
  unchanged), CW-in/CCW-out, pivot clamp; `plugin/test/zoom_cursor_test.cpp`
  — the verb (CW narrows, CCW widens, no undo, no args); and the
  `oracle/scenarios/hid_zoom.lua` oracle scenario — `TIMELINE`-hold zoom
  shrinks the span CW around the cursor (`q` stable within ~2 px of the
  pixel snap), widens CCW.

## 8. Decision points (confirm before implementing)

1. ~~**Scrl** — adopt screen-constant pixels per detent or leave it fixed?~~
   **Resolved:** screen-space-constant, same `k_px / pxPerSec` scale as jog.
2. ~~**`TIMELINE.turn`** — rebind to zoom, displacing `select_step`?~~
   **Resolved:** rebind to `zoom_cursor`; `select_step` is removed (a new
   selection method replaces the feature) — code + tests deleted.
3. ~~**Config names + defaults**~~ **Resolved:** a single
   `wheel.pixels_per_detent` = **0.25** (bigger = faster); the zoom
   granularity is derived from it (§4.2) — no second key.
   ~~Open sub-item: extend the `[tuning]` parser for floats (vs scaled
   integers)?~~ **Resolved:** the `[tuning]` parser gained a `strtod` path
   for the float key; the other `[tuning]` keys stay integer-only.
4. **Zoom direction + granularity** — **Resolved:** CW (positive delta) =
   zoom in; span fraction per detent `f = k_px / (pxPerSec · W)`.
   ~~Open sub-item: confirm **no** undo block for view changes (proposed:
   yes).~~ **Resolved:** no `Undo_Begin/End` — the arrange view is not an
   undoable edit.
5. ~~**Zoom pivot**~~ **Resolved:** the **edit cursor**
   (`GetCursorPosition`), confirmed.

## 9. Candidate implementation plan (after decisions)

1. Add `REAPERAPI_WANT_GetHZoomLevel` to `reaper_api.h`; expose
   `PixelsPerSecond()` through a seam + thread `wheel.pixels_per_detent` into
   `JogEngine`; replace `kSecondsPerWheelUnit` with
   `(delta / kUnitsPerRev) · (k_px / pxPerSec)` (jog_engine.h, session.cpp,
   reaper_sinks.cpp).
2. Point `Router::OnWheel` (Scrl) at the same `k_px / pxPerSec` scale.
3. Add the `zoom_cursor` turn verb (verb_registry.cpp): new `VerbContext`
   field `px_per_detent`; new `IProject` reads `PixelsPerSecond()` +
   `ViewSpanSeconds()`; new `IActionSink` span-zoom primitive (implemented in
   `ReaperActionSink` via `GetSet_ArrangeView2` + `UpdateArrange`); CW = in,
   `f = k_px / (pxPerSec · W)` per detent, pivot = edit cursor; no undo
   block.
4. **Remove** `select_step`: `SelectStepVerb` + `kSelectStepTicks` +
   registration (verb_registry.cpp:329, 357-369, 791-796), the
   `TIMELINE.turn` row in `default_bindings.ini` (→ `zoom_cursor`), and the
   test cases listed in §7.
5. Add the `[tuning]` key + float parsing (bindings.{h,cpp}); update the
   `[tuning]` block in `default_bindings.ini`.
6. Re-baseline `jog_engine_test.cpp`, `wheel_dispatcher_test.cpp`,
   `hid_wheel.lua`; add oracle/unit coverage for the zoom math.

## Sources

- Current plugin source (anchored above): `plugin/src/jog_engine.h`,
  `wheel_dispatcher.h`, `router.{h,cpp}`, `reaper_sinks.cpp`,
  `verb_registry.{h,cpp}`, `verbs.{h,cpp}`, `session.{h,cpp}`,
  `reaper_project.cpp`, `reaper_api.h`, `bindings.{h,cpp}`,
  `default_bindings.ini`, `src/speed_editor/key_map.h`.
- Hardware wheel scale: `docs/notes/m1-wheel-scale.md`.
- REAPER SDK (vendored): `vendor/reaper-sdk/sdk/reaper_plugin_functions.h`.
- ReaScript API reference (zoom API cross-check):
  https://www.extremraym.com/cloud/reascript-doc/
