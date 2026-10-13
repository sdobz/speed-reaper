# Ralph record: 23-m4-shuttle

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- The Windows cross build inside `tools/check` snapshots the repo from git: new plugin files must be `git commit`ted before `tools/check` can see them (modified tracked files are seen, untracked files are not).
- Headless REAPER oracle: `Main_OnCommand(40045)` (transport stop command) blocks the script/defer queue forever — scenarios hang with no probe.json. Use `reaper.OnStopButton()` / `reaper.OnPlayButtonEx(0)` for transport control (verified working headless, M4 task 3).
- main.cpp: SDK/Swell `min`/`max` macros are in force after `reaper_plugin.h` — `std::min` / `#include <algorithm>` there breaks the -Werror build; use ternaries.


## Task: 1. `ShuttleRate` (pure)

### Attempt 1 — 2026-10-12T21:03:09Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Design §8 (docs/plugin-design-m1-m3.md L155): rate = 1 + 7·s²(3−2s), s = (|offset|−40)/(4096−40) clamped to [0,1], 0 in dead band |offset|<40, sign of offset.
- Created plugin/src/shuttle.h/.cpp (`double ShuttleRate(std::int32_t offset)`), added shuttle.cpp to PLUGIN_PURE_SOURCES (kept list alphabetical), added plugin/test/shuttle_test.cpp (test/CMakeLists globs `*_test.cpp` → ctest name `shuttle_test`).
- `cmake` is not on plain PATH; run everything through `tools/check` (it execs `nix develop`).

Dead ends:
- First `tools/check shuttle` run: Linux build + `shuttle_test` all passed, but exit 1 — the Windows cross build (`nix build .#reaper-plugin-windows`) packs a git-based source snapshot that includes *modified tracked* files but not *untracked* ones, so CMake found the new CMakeLists.txt (referencing shuttle.cpp) but not shuttle.cpp itself → "Cannot find source file". Fix: `git commit` the new plugin files (commit 9f9b726), then re-run.

Next:
- DONE: bgrun `check-shuttle2` exit 0 (shuttle_test passed, WINDOWS ORACLE PASS static, ORACLE PASS host). Line 21 ticked. Plugin files in commit 9f9b726.

Result: exit 0; done; 4 files changed, 57 insertions(+), 12 deletions(-); log: logs/23-m4-shuttle-L21-a1-1791838989818.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
M SMOKE-TEST.md
 M tools/reaper-oracle
 M vibes/.ralph/23-m4-shuttle.md
 M vibes/23-m4-shuttle.md
 M vibes/30-backlog.md
?? docs/ReaperUserGuide781c/
?? docs/plugin-design-m5-m9.md
?? oracle/scenarios/ripple_del.lua
?? vibes/31-m5-targeting.md
?? vibes/32-m6-simple-verbs.md
?? vibes/33-m7-hold-rotate.md
?? vibes/34-m8-led-bar.md
?? vibes/35-m9-crossfade-spike.md
?? vibes/36-m10-bindings.md
```

## Task: 2. Spike actions: probe the playrate API

### Attempt 2 — 2026-10-12T21:07:38Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- API subset: `Master_GetPlayRate(ReaProject*)` (L108, pass nullptr), `Master_NormalizePlayRate(double, bool)` (L112, not needed), `CSurf_OnPlayRateChange(double)` (L268, projectless — sets current project's rate).
- main.cpp: added 4 spike actions (rate2=2.0, rate8=8.0, reverse=-1.0, reset=1.0+stop 40045), `REAPERAPI_WANT_CSurf_OnPlayRateChange` + `REAPERAPI_WANT_Master_GetPlayRate`, `SpikeState` global, timer prints `spike rate=%f read=%f state=%d pos=%f` (ShowConsoleMsg + Trace) ~300 ms after; reverse also prints pos0/pos1.
- Spike handling in onHookCommand2 placed before the `!g_engine` guard so probes work standalone; stop via Main_OnCommand(40045) (transport stop), play-start via 1007 per task.

Next:
- DONE: `tools/check` exit 0 (10/10 tests, WINDOWS ORACLE PASS static, ORACLE PASS host); verify command incl. `strings ... | grep -q speed_reaper_spike_reverse` passed; line 38 ticked; commit 426c46b (+ tick amend).

Result: exit 0; done; log: logs/23-m4-shuttle-L38-a2-1791839258625.log

### Done — 2026-10-12 — 2 attempt(s)
Files:
```
M vibes/.ralph/23-m4-shuttle.md
```

## Task: 3. Run the spike under the oracle and write the findings

### Attempt 3 — 2026-10-12T21:11:08Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Spike actions fire from REAPER via `Main_OnCommand(NamedCommandLookup("_speed_reaper_spike_*"), 0)`; cmds are 53003-53006 in the oracle home. Plugin prints `spike rate=%f read=%f state=%d pos=%f` ~300 ms later into `build/oracle/trace_spike_rate.txt`; scenario can read that file with `io.open(os.getenv("ORACLE_OUT") .. "/trace_spike_rate.txt")` (pattern from buttons.lua).
- First measured data point (broken run): reverse's `spike rate=-1.000000 read=1.000000 state=1` — CSurf_OnPlayRateChange(-1.0) is NOT reflected by Master_GetPlayRate (reads back 1.0). rate2/rate8 readings lost because all 4 actions fired within 1 ms and the plugin has a single `g_spike.pending` slot (last one wins).
- RunSpike starts transport (1007) when stopped; RunSpikeReset stops (40045) instead.

Dead ends:
- For-loop over the 4 actions with `wait_ms(600, ...)` between them: wait_ms is NON-blocking (reaper.defer), so all 4 Main_OnCommands fired in ~1 ms, the pending slot got overwritten 3×, only reverse produced a result line, and REAPER never quit (no probe.json → timeout). Fix: chain actions through the wait's on_done (sequential state machine) — done in oracle/scenarios/spike_rate.lua.
- **`Main_OnCommand(40045)` (transport stop) blocks forever in headless REAPER under the oracle** — the whole Lua VM freezes (defer dies), only the plugin timer thread keeps running, and the scenario times out with no probe.json (repro: oracle/scenarios/dbg8.lua; found via progress-file logging in dbg7.lua). The play command (1007) is fine. Fix: plugin RunSpikeReset now calls `OnStopButton()` (REAPER API subset §2, works headless — verified in dbg9.lua: state 1→0). `REAPERAPI_WANT_OnStopButton` added to main.cpp.

Facts (measurements, REAPER 7.81 linux headless, empty project, cursor 10 s):
- REAPER clamps master playrate to [0.25, 4.0]: requested 2.0 → read back 2.0; 8.0 → **4.0**; −1.0 → **0.25** (not 1.0, not negative). `CSurf_OnPlayRateChange` + `Master_GetPlayRate` are the APIs used.
- Playhead (play_a/play_b 300 ms apart) shows motion: rate2 ~2×, rate8 ~4× (clamped), "reverse" ~4× forward (still the clamped 8× era rate; NO reversal), reset → state 0.
- `reaper.GetCursorPosition()` (edit cursor) does NOT follow the playhead during playback — scenario now also samples `reaper.GetPlayPositionEx(0)`; motion/reversal is judged on the playhead.
- dbg6–dbg9 scenarios are throwaway repros kept in oracle/scenarios/ (delete if you like).

Next:
- DONE: final spike_rate run ORACLE PASS (all 4 trace lines present; table data in the doc); docs/notes/m4-shuttle-api.md written (measurements table + answers + Limits + 3 candidate approaches, nothing implemented); bg job `check-spike` (tools/check) exit 0 — 10/10 tests, WINDOWS ORACLE PASS static, ORACLE PASS host; Verify command passes; line 51 ticked. Plugin change (OnStopButton) is committed; scenario + doc committed with the tick.

Learnings-worthy (also under ## Learnings):
- Headless REAPER: `Main_OnCommand(40045)` (transport stop) blocks the entire script/defer queue forever; use `OnStopButton()` / `OnPlayButtonEx(0)` instead (works headless). 40044/1007 for start-play are fine.
- REAPER master playrate clamps to [0.25, 4.0]; `Master_GetPlayRate` is the truthful read-back.
- `GetCursorPosition` (edit cursor) does not follow the playhead during playback; use `GetPlayPositionEx(0)` for motion checks.

Result: exit 0; done; log: logs/23-m4-shuttle-L51-a3-1791839468547.log

### Done — 2026-10-12 — 3 attempt(s)
Files:
```
M vibes/.ralph/23-m4-shuttle.md
```

## Task: 4. Decide the shuttle approach by rule (no human)

### Attempt 4 — 2026-10-12T21:37:29Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Rule eval from docs/notes/m4-shuttle-api.md: −1× reverse reads back 0.25 and the playhead moves *forward* → rule 1 ("playrate only") fails on reverse; forward works (2.0→2.0, 8.0→clamped 4.0, forward) → **rule 2 wins**: forward via playrate, reverse via stepped seek.
- Wrote docs/plugin-design-m4-shuttle-impl.md: rate setter CSurf_OnPlayRateChange (read-back Master_GetPlayRate(nullptr)); reverse via SetEditCurPos(pos − |rate|·dt, false, 1) — same call ReaperPlayheadSink::ScrubBySeconds already uses (main.cpp L128); tick period = existing kTimerMs=8 ms engine steps in the ~30 Hz REAPER timer (main.cpp L80, L206); STOP/PLAY = hard stop via OnStopButton (never 40045) + rate 1.0 + exit SHTL; JOG/SCRL tap = rate 1.0 + hand off to relative mode; dead band 0 → OnStopButton, no rewind.

Next:
- DONE: verify passed, line 63 ticked.

Result: exit 0; done; 2 files changed, 13 insertions(+), 1 deletion(-); log: logs/23-m4-shuttle-L63-a4-1791841049690.log

### Done — 2026-10-12 — 4 attempt(s)
Files:
```
M vibes/.ralph/23-m4-shuttle.md
 M vibes/23-m4-shuttle.md
?? docs/plugin-design-m4-shuttle-impl.md
```

## Task: 5. Wire SHTL per the decision

### Attempt 5 — 2026-10-12T21:39:12Z — budget 1800s
- Facts: Decision doc (task 4) = rule 2: forward via `CSurf_OnPlayRateChange` clamped to 4.0 (REAPER clamps 0.25…4.0), reverse via stepped `SetEditCurPos(pos − |rate|·dt, false, 1)` per 8 ms engine tick, dead band → `OnStopButton` + rate 1.0 (no rewind), STOP/PLAY in SHTL = HardStop + exit to prior mode, JOG/SCRL tap = rate 1.0 + relative mode. Doc does NOT say to remove spike actions → kept.
- Facts: `HidWheelSource::AbsolutePosition()` already exists (M4 hook, hid_wheel_source.h L41). `ShuttleRate` in plugin/src/shuttle.{h,cpp} (task 1). Doc's `GetPlayPositionEx(0)` → added `REAPERAPI_WANT_GetPlayPositionEx`.
- Design: new `IActionSink::ApplyShuttleRate(double)` + `IActionSink::HardStop()`; Router gains `OnShuttleOffset(int32_t)` (no-op outside Shtl; 0 → one HardStop per band entry, re-armed on nonzero rate or any mode change; ±2068 → ±4.5 passed to sink). Router tracks `shuttle_prev_` for STOP/PLAY exit (initial Jog). main.cpp: ReaperActionSink implements both (stopped transport parks forward/reverse; JOG/SCRL hand-off = rate 1.0); onTimer engine-tick loop calls `g_router.OnShuttleOffset(g_hid.AbsolutePosition())` each 8 ms step. Fake sinks in router_test.cpp and verbs_test.cpp updated (new pure virtuals).
- Dead ends: `#include <algorithm>` + `std::min` in main.cpp fails the -Werror build — the SDK/Swell min/max macros are in force after reaper_plugin.h; use a ternary (`rate < 4.0 ? rate : 4.0`).
- Done: `tools/check router` rc=0 (job check-router2, log vibes/.ralph/logs/check-router2.log): -Werror build clean, router_test passes (dead band → no rate call + one HardStop; ±2068 → ±4.5; STOP/PLAY in SHTL → HardStop + exit to prior mode; JOG tap → rate 1.0), Windows cross build PASS, oracle host PASS. Full ctest: 10/10. Line 74 ticked.

Result: exit 0; done; log: logs/23-m4-shuttle-L74-a5-1791841152951.log

### Done — 2026-10-12 — 5 attempt(s)
Files:
```
M vibes/.ralph/23-m4-shuttle.md
```

## Task: 6. Oracle scenario: `shuttle`

### Attempt 6 — 2026-10-12T21:48:00Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Fake device wheel reports ECHO the device-side wheel mode (`FakeTransport::Write` 0x03 sets `wheelMode_`, wheel report byte 2 = mode; src/speed_editor/fake_transport.cpp L196, L235). The worker treats mode 1/3 as ABSOLUTE (`abs_.store`), 0/2 as relative. The plugin writes protocol mode 3 after the SHTL press (ApplyModeToHid → RequestWheelMode, protocol: Shtl=3), so plain `at <ms> wheel <value>` lines after the SHTL tap carry absolute deflection directly — no accumulation needed in the script.
- Key codes: SHTL=0x1C, JOG=0x1D, SCRL=0x1E, STOP/PLAY=0x3C (src/speed_editor/key_map.h).
- `ShuttleRate(4096)=8.0`, `ShuttleRate(-2068)=-4.5` (s=0.5 exact).
- Task text "Master_GetPlayRate ≈ 8" is NOT literally achievable: the chosen approach (rule 2) clamps the request to 4.0 at the API boundary and REAPER itself clamps read-back to [0.25,4.0] (spike: 8.0 requested → 4.0 read). The scenario asserts the documented 4.0 read-back + >2×-realtime playhead motion and records the request via the `shuttle rate=` trace line.
- Reverse after the dead-band pause needs a playing transport (main.cpp parks forward/reverse while stopped, per the decision doc: "reverse while stopped is scrub, JOG/SCRL's job"). shuttle.lua resumes play with `reaper.OnPlayButtonEx(0)` at ~lua 1900 ms (script ~1860), before the script's `wheel -2068` at 2300. Net reverse motion ≈ −4.5× seek + 1.0× transport = ~−3.5×.
- Clocks: script t=0 at plugin load (fake Open); lua t=0 ~40 ms later → lua t ≈ script t − 40 ms; all check windows carry ≥100 ms margin. 1500 ms warm-up (buttons/spike pattern) before the timeline.
- JOG-tap "rate 1.0" after the reverse phase is trivially true (master rate was already 1.0 during stepped reverse); the substantive assertion is the trace `mode Shtl` → later `mode Jog` + `led jog=1`.
- STOP/PLAY while back in Jog runs cmd 40044 (toggle) — headless-safe (buttons scenario precedent); 40045 never used.
- Files: oracle/fake/shuttle.txt, oracle/scenarios/shuttle.env, oracle/scenarios/shuttle.lua; `shuttle` appended to the default suite in tools/reaper-oracle (LAYER==all list + header comment).
- No plugin changes needed for task 6 (task 5 wiring already traces `shuttle rate=`, `shuttle reverse=`). Scenario-only → no rebuild, no git-snapshot concern; reaper-oracle reaper layer uses the existing build/plugin/reaper_speed_reaper.so (verify it contains the M4 sink before trusting a PASS).

Result: exit 0; progress; 2 files changed, 22 insertions(+), 4 deletions(-); log: logs/23-m4-shuttle-L85-a6-1791841680150.log

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 6 attempt(s), last: exit 0, progress
Log: logs/23-m4-shuttle-L85-replan-1791842803633.log

## Task: 7. Test instructions: SMOKE-TEST.md "M4 — shuttle"

### Attempt 1 — 2026-10-13T00:57:13Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Appended `## M4 — shuttle` after M3 in SMOKE-TEST.md (~90 lines): oracle covers numbers (`--scenario shuttle`), hand test covers only E2 (sound at 2×/4×/reverse, feel of curve/dead-band/center-park). Per step: action, expected, ☐ pass / ☐ fail + notes line; ends with a pass bar.
- Feel-tuning step names the three knobs in `ShuttleRate` (plugin/src/shuttle.cpp): dead-band `40` (≈1% of ±4096 travel), max-rate gain `7.0` (top 8.0; audible forward top is REAPER's 4.0× cap — no knob raises it), curve `s²(3−2s)`; human reports numbers (e.g. "want ±80") not impressions, to docs/notes/m4-observations.md.
- Key behaviors documented: dead band = hard stop (one per band entry, no rewind); while stopped the rate law parks (forward deflection does NOT restart playback — resume = JOG tap, STOP/PLAY, re-enter SHTL); STOP/PLAY in SHTL = HardStop + exit to prior mode (never a play toggle); JOG/SCRL tap = rate 1.0 + relative mode; reverse while stopped is inert; section supersedes M2 step 4 (SHTL was documented inert there).
- Verify `grep -q '^## M4 — shuttle' SMOKE-TEST.md` passes; line 96 ticked. No code touched → no build, no git-snapshot concern.

Next:
- DONE: nothing; task complete.
