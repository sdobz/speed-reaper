# Ralph record: 20-m1-wheel-playhead

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).


## Task: 1. Rename the plugin's `WheelEvent` to `WheelSample`

### Attempt 1 — 2026-10-12T02:45:48Z — budget 600s
- Facts:
  - Tree was clean at start; no partial work from earlier attempts.
  - `WheelEvent` occurrences in plugin: `plugin/src/jog_engine.h` (struct def + 2 signatures), `plugin/src/jog_engine.cpp` (2 uses), `plugin/test/jog_engine_test.cpp` (1 use). Renamed all via `sed s/WheelEvent/WheelSample/g` on exactly those three files.
  - `src/speed_editor/event_types.h` also defines its own `WheelEvent` — left untouched per task instructions; the verify grep only scans `plugin/src` and `plugin/test`, so it doesn't interfere.
  - `main.cpp`/`trace.*` don't reference the struct, so nothing else in the plugin needed changes.
  - `tools/check jog_engine` passes (build + jog_engine_test + oracle PASS); `grep -rn WheelEvent plugin/src plugin/test` finds nothing. Full verify command exits 0.
- Dead ends:
  - None.
- Next:
  - Task complete; checkbox ticked. Move on to the next task in the file.

Result: exit 0; done; 4 files changed, 7 insertions(+), 7 deletions(-); log: logs/20-m1-wheel-playhead-L26-a1-1791773148918.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M plugin/src/jog_engine.cpp
 M plugin/src/jog_engine.h
 M plugin/test/jog_engine_test.cpp
 M vibes/20-m1-wheel-playhead.md
?? vibes/.ralph/20-m1-wheel-playhead.md
```

## Task: 2. Fix the demo wiring: `SummedWheelSource`

### Attempt 2 — 2026-10-12T02:46:37Z — budget 600s
- Facts:
  - At start, only the record file was dirty; `main.cpp` already had the demo action driving `g_demo_source` directly, but the engine was built on `g_demo_source` alone and `JogEngine` still owned `demo_`/`ToggleDemo()`/etc.
  - Added `NullWheelSource` + `SummedWheelSource` (header-only, refs to both sources) to `plugin/src/jog_engine.h`; removed `demo_`, `demo_on_`, `ToggleDemo()`, `DemoActive()`, `Demo()` from `JogEngine`. No other references existed (`grep -rn "ToggleDemo|DemoActive|Demo()" plugin/` clean).
  - `main.cpp`: `NullWheelSource g_null;` + `SummedWheelSource g_wheel_source(g_demo_source, g_null);`; engine built on `g_wheel_source`.
  - Test: sums two `FixedSource`s (7 + -3 = 4); triggered `DemoWheelSource` through `SummedWheelSource(demo, null)` into engine moves `SumSink` forward, then settles.
  - `tools/check jog_engine` exits 0 (test + oracle PASS).
- Dead ends:
  - `SummedWheelSource src2(demo, NullWheelSource())` — cannot bind rvalue to `IWheelSource&` param; use a named local.
  - Settle check right after the turn: the low-pass velocity (tau=50 ms) keeps drifting for ~800 ms, so the sink moves >1e-9 within a short window. Fix: coast 260 ticks (turn 1.25 s + 800 ms ≈ 16 tau) before measuring `settled2`, then CHECK_NEAR over 10 ticks — same pattern as the pre-existing test.
- Next:
  - Task complete; checkbox ticked.
