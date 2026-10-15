# 40 — Code clean-up (no behavior change)

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`). Never guess an id.

Builds on `34-m8-led-bar.md`. The plugin grew one milestone at a time and it shows: `main.cpp` is 531 lines doing six jobs, dead demo/spike code still ships, three wheel decorators route one delta, `IProject` carries verb logic, and comments cite planning docs. This file removes the chaos **without changing what any button does**. The mapping-layer merge (verb tables → registry) is `41-action-registry.md`, not here.

Rules for every task:
- **No behavior change.** The full reaper-layer oracle suite must pass after every task that touches `plugin/src` (`tools/reaper-oracle --layer reaper`), minus scenarios this file explicitly deletes.
- Delete, don't comment out. No compatibility shims for removed code.
- New comments say *what and why*, never "M4 task 3" / "design §6".
- Every Verify starts with `tools/check`.

### 1. Repo hygiene

- `.gitignore`: add `vibes/.ralph-logs/`, `oracle/__pycache__/` (already covered by `__pycache__/`; check), `build/oracle/`. `git rm -r --cached vibes/.ralph-logs` (keep files on disk).
- Commit the untracked `oracle/scenarios/spike_xfade.lua` (`41-action-registry.md` uses it).
- Delete one-off probes not in the suite: `oracle/scenarios/probe_sv2.lua` … `probe_sv5.lua`, `probe_smth.lua`, `spike_rate.lua`. Before deleting each, `rg <name>` across `tools/ docs/ vibes/ SMOKE-TEST.md`; a doc that cites it gets the citation replaced by the finding it produced (one sentence), not a dangling path.

**Verify**
```bash
tools/check && test -z "$(git ls-files vibes/.ralph-logs)" && ! ls oracle/scenarios/probe_* oracle/scenarios/spike_rate.lua 2>/dev/null && git ls-files --error-unmatch oracle/scenarios/spike_xfade.lua
```

- [x] Done

### 2. Delete the hello-world demo and the M4 spike code

- `main.cpp`: remove `g_cmd_demo/fwd/back`, the four `g_cmd_spike_*`, `SpikeState`/`g_spike`, `RunSpike`, `RunSpikeReset`, the spike probe block in `onTimer`, their `RegisterAction` calls and `onHookCommand2` branches. Drop `REAPERAPI_WANT_*` lines that only those used (e.g. `Master_GetPlayRate` if nothing else calls it — grep).
- `jog_engine.h/.cpp`: delete `DemoWheelSource` and any of `SummedWheelSource` / `NullWheelSource` / `JogEngine::NudgeSeconds` left without a caller; update `jog_engine_test.cpp`.
- Oracle: remove `jog_demo` from `REAPER_SCENARIOS` in `tools/reaper-oracle` (and its comments, lines ~10 and ~227) and delete `oracle/scenarios/jog_demo.lua`; `hid_wheel` already proves wheel → playhead.
- Replace the stale header comment of `main.cpp` with a 5-line description of what the file does now.
- SMOKE-TEST.md: delete the "REAPER plugin: hello world" section; keep any prerequisite steps it holds by moving them to "Prerequisites".

**Verify**
```bash
tools/check && ! rg -q 'spike|jogdemo|DemoWheelSource|scrub_fwd' plugin/src tools/reaper-oracle && tools/reaper-oracle --layer reaper
```

- [x] Done

### 3. One REAPER API list

- Create `plugin/src/reaper_api.h`: the single `REAPERAPI_MINIMAL` + every `REAPERAPI_WANT_*` line (one per function, comment = caller), then `#include "reaper_plugin_functions.h"`. `main.cpp` defines `REAPERAPI_IMPLEMENT` before including it; `reaper_project.cpp` (and the sink file of task 4) include it without. Delete the duplicated lists and the "keep in sync" comments.
- Add a check to `tools/check` (or a small script it calls): every REAPER function called in `plugin/src/*.cpp` that appears in `oracle/sdk_api_names.txt` has a `REAPERAPI_WANT_` line. Fail with the missing names.

**Verify**
```bash
tools/check && test "$(rg -c 'REAPERAPI_WANT_' plugin/src | wc -l)" -eq 1 && tools/reaper-oracle --layer reaper --scenario load
```

- [x] 3. One REAPER API list (Verify without rg)
  <!-- ralph: replanned=1 -->
  Attempt 3 already built everything and it is in the tree: `plugin/src/reaper_api.h` (MINIMAL + 39 `REAPERAPI_WANT_*` lines, comment = caller), `main.cpp` (`#define REAPERAPI_IMPLEMENT` + include), `reaper_project.cpp` (plain include), and `tools/check_api_want` wired into `tools/check` before the build. Do not recreate any of it.
  Sanity-check only: exactly one file under `plugin/src` matches `REAPERAPI_WANT_` (`grep -rl 'REAPERAPI_WANT_' plugin/src`), and no duplicated list or "keep in sync" comment remains in `main.cpp`/`reaper_project.cpp`.
  The previous Verify failed only because `rg` is not on PATH in the verify environment; the corrected Verify below uses `grep` instead.
  Run Verify until it exits 0 (builds are cached from attempt 3, so this is a few minutes), then tick.
  Verify: `tools/check && test "$(grep -rl 'REAPERAPI_WANT_' plugin/src | wc -l)" -eq 1 && tools/reaper-oracle --layer reaper --scenario load`

### 4. Move the REAPER sinks out of `main.cpp`

- Move `ReaperPlayheadSink` and `ReaperActionSink` to `plugin/src/reaper_sinks.h/.cpp` (REAPER-backed, next to `reaper_project.cpp`, not in `PLUGIN_PURE_SOURCES`). No logic change.

**Verify**
```bash
tools/check && ! rg -q 'class Reaper(Playhead|Action)Sink' plugin/src/main.cpp && tools/reaper-oracle --layer reaper
```

- [x] Done

### 5. Pure `Session`: the timer loop becomes testable

- Create `plugin/src/session.h/.cpp` (pure): owns `GestureEngine`, `Router`, `VerbRouter`, `JogEngine`, LED-mask state and the connect edge. Input interface: `IDevice` (pop button/log, connected, absolute position, wheel tick, request mode/LEDs) — implemented by `HidWheelSource` and a test fake. One method `void Tick(std::int64_t now_ms)` holds everything `onTimer` does today: button routing, gesture pop → verbs, mode change → device, reconnect re-send, LED poll every ~250 ms (time-based, not tick-count), fixed-step engine catch-up with the 250 ms cap and the `timer gap` trace.
- `onTimer` becomes: compute `now_ms`, `g_session->Tick(now_ms)`, drain logs to the console.
- `plugin/test/session_test.cpp`: fake device + `fake_project.h`: reconnect re-sends mode + LEDs; LED mask pushed only on change; a stall > 250 ms catches up at most the cap; a key press reaches the verb sink.

**Verify**
```bash
tools/check session && test "$(sed -n '/^void onTimer/,/^}/p' plugin/src/main.cpp | wc -l)" -le 15 && tools/reaper-oracle --layer reaper
```

- [x] Done

### 6. `WheelDispatcher` replaces the wheel decorators

- One pure class decides where each tick's wheel input goes, in priority order: active hold → `GestureEngine::OnWheel`; else by mode: Jog → `JogEngine`, Scrl → `Router::OnWheel`, Shtl → `Router::OnShuttleOffset(absolute)`. Delete `ModeGatedSource`, `HoldGatedSource`, and whichever of `SummedWheelSource`/`NullWheelSource` remain; shuttle no longer needs a separate call in the catch-up loop.
- Port the routing assertions from `router_test.cpp`/`session_test.cpp` into `wheel_dispatcher_test.cpp` (one case per branch, plus "hold wins over every mode").

**Verify**
```bash
tools/check 'wheel_dispatcher|router|session' && ! rg -q 'ModeGatedSource|HoldGatedSource' plugin && tools/reaper-oracle --layer reaper
```

- [x] Done

### 7. Merge `hold_nudge` into `nudge`

- Move `HoldNudge` into `nudge.h/.cpp`; delete `hold_nudge.*`; merge `hold_nudge_test.cpp` into `nudge_test.cpp`; update `PLUGIN_PURE_SOURCES` and `plugin/test/CMakeLists.txt`.

**Verify**
```bash
tools/check nudge && ! ls plugin/src/hold_nudge.* 2>/dev/null
```

- [x] Done

### 8. `IProject` down to primitives

- Goal: `IProject` exposes REAPER *state*, not verbs, so `fake_project.h` stops re-implementing verb logic. Move out of `IProject` into pure functions in `plugin/src/project_ops.h/.cpp` built on primitives:
  - `DuplicateItemsAbove` → primitives `InsertTrackAbove(track) -> int`, `CopyItemToTrack(id, track) -> int` (REAPER side: `InsertTrackAtIndex` + item state chunk copy; keep the exact current behavior of `reaper_project.cpp`).
  - `ToggleIsolateTracks` / `ToggleSoloTracks` / `IsolateActive` / `SoloActive` → primitives `GetTrackFlag(track, flag)` / `SetTrackFlag(...)` for solo/height/visibility as used today; the saved-state for the re-tap restore lives in the pure op, not the REAPER class.
  - `ReadLedState` → stays only if it is a pure aggregate of primitives (`ToggleState(cmd)`, `RippleMode()`, takes); then move it to `project_ops` too.
- Add the new `REAPERAPI_WANT_` lines to `reaper_api.h`. Unit-test each op against `fake_project.h` (which shrinks: primitives only).

**Verify**
```bash
tools/check && ! rg -q 'DuplicateItemsAbove|ToggleSoloTracks|ToggleIsolateTracks' plugin/src/project.h && tools/reaper-oracle --layer reaper
```

- [ ] Done

### 9. Comment pass

- In `plugin/src`, rewrite every comment that cites a milestone, task or doc section (`M[0-9]`, `task [0-9]`, `design §`, `§[0-9]`) into the fact it stands for. Keep doc pointers only for non-obvious protocol/API facts, and point at `docs/` files that `42-release-prep.md` keeps (`speed-editor-capabilities.md`, `reaper-api-subset.md`, `docs/notes/*`).
- Command-id comments stay (`// Item: Split items at edit or play cursor`), they are the id's name.

**Verify**
```bash
tools/check && ! rg -n 'M[0-9]+\b|task [0-9]|design §|§[0-9]' plugin/src
```

- [ ] Done
