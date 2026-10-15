# Ralph record: 41-action-registry

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/check` includes the Windows oracle, which `nix build`s the packed working tree: any new .cpp listed in CMakeLists must be committed (tracked) or CMake Generate fails with "Cannot find source file". Same for files read at configure time: plugin/src/default_bindings.ini is file(READ) by CMakeLists, and the nix build failed until the .ini was tracked.
- Lambda factories with a multi-statement body returning a braced `VerbBuildResult` need an explicit `-> VerbBuildResult` (return-type deduction doesn't work from braced-init-lists).
- In test CHECK macros, wrap `x == std::vector<T>{a, b}` comparisons in extra parentheses — commas inside `{}` split the macro argument.
- Declaring any copy ctor (even `= delete`) suppresses the implicit default AND move ctors — when making a class non-copyable, also add `Ctor() = default;` / `Ctor(Ctor&&) = default;` or `return local;` by value breaks.


## Task: 1. `VerbRegistry` (pure)

### Attempt 1 — 2026-10-15T04:00:48Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; log: logs/41-action-registry-L52-a1-1792036848217.log

### Replan (depth 0) — SPLIT op=AND → "Registry core + 10 tap verbs + basic tests", "Turn verbs: select_step + nudge", "wheel + transport play_stop (shared WheelState)"
Trigger: 1 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Why: Attempt 1 made no progress at all: the worker read the code fine, then drafted the whole ~600-line design (14 verbs + core + tests) in its message and monologued to the 180s timeout without writing a file. The task is 2-3x the one-sitting size and leaves design details (gesture vocabulary, error shape, wheel-state ownership) open, which is exactly what sends this model into circles.
Rejected: BUDGET: no progress in the record (no files, no record bullets), so more time buys the same monologue. REWRITE: the task text and its Verify are sound — the problem is scope and unpinned design, not clarity, and a rewrite can't shrink it. SPIKE: no open question needs a doc; the milestone Design section plus the attempt's own monologue already settle the API, so a spike round-trip adds nothing. BLOCK: nothing needs a human decision.
Log: logs/41-action-registry-L52-replan-1792037167047.log

## Task: Registry core + 10 tap verbs + basic tests

### Attempt 1 — 2026-10-15T04:12:34Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Created plugin/src/verb_registry.h (BindingGesture + GestureName, VerbContext, Verb, VerbBuildResult, VerbSpec, VerbRegistry, MakeDefaultRegistry) and plugin/src/verb_registry.cpp (10 verbs: action, targeted, ripple_delete, select_cursor_item, take, volume_readout, smooth_cut, place_on_top, isolate_tracks, solo_tracks; logic moved from verbs.cpp free functions; view verbs own their IsolateView/SoloView).
- Added src/verb_registry.cpp to PLUGIN_PURE_SOURCES in plugin/CMakeLists.txt (between trace.cpp and verbs.cpp). Test is auto-globbed.
- Int args parsed via strtol (full-string check + INT range); factory errors start with the verb name ("take: ...", "action: ..."); Build errors: "unknown verb 'X'" / "verb 'X' does not accept gesture 'y'".
- Created plugin/test/verb_registry_test.cpp: 10 names resolve + Build; gesture sets enforced (action/targeted press+tap+double; ripple_delete/take tap+double; others tap); bad args rejected (take 0/abc//extra, action abc//, ripple_delete extra); behavior spot-checks per verb (FakeSink + FakeProject pattern from verbs_test.cpp).
- All 10 factory lambdas need explicit `-> VerbBuildResult` return type (C++ can't deduce from a braced init-list). No-arg verbs reject extra args via CheckNoArgs ("<name>: takes no arguments").
- Dead ends: untracked source files break `tools/check`'s Windows oracle: nix packs the tree but the CMake Generate fails on the missing .cpp ("Cannot find source file") → commit before re-running the check. Also `CHECK(x == std::vector<int>{a, b})` splits the macro on the commas inside {} — wrap the whole expression in extra parens.
- Done: `tools/check verb_registry` exits 0 (ctest, windows-oracle static, oracle host). Committed as 4703642.

Result: exit 0; done; log: logs/41-action-registry-L54-a1-1792037554772.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/41-action-registry.md
```

## Task: Turn verbs: select_step + nudge

### Attempt 1 — 2026-10-15T04:22:37Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Clean tree (only record modified); registry core + 10 tap verbs committed in 4703642.
- StepSelection / kSelectStepTicks / kHoldRows logic still lives only in verbs.cpp — moving into verb_registry.cpp anonymous namespace, no header change.
- nudge.cpp is already in PLUGIN_PURE_SOURCES, so verb_registry.cpp can use nudge.h directly.
- FakeProject::SetSelectedItems records, not applies: tests must copy the last selected_calls entry into items[].selected between verb calls (verbs_test.cpp ~line 252 pattern).
- Mode map: move/"Speed Reaper: move"/NudgeMove, trim_in/"trim in"/NudgeTrimIn, trim_out/"trim out"/NudgeTrimOut, slip_src/"slip src"/NudgeSlipSrc, slip_dest/"slip dest"/NudgeSlipDest, volume/"audio level"/NudgeVolume.
- select_step only overrides Step (no Begin/End — the old router had no TIMELINE HoldStart state); nudge keeps an active_ flag set by Begin (no targets → inactive, no undo block, Step/End no-ops).
- Dead ends: my first trim_out test expected `len -= 1/fps` — wrong: NudgeTrimOut is `len += steps*step` (the moved-verbatim math). Fixed the expectation, test passes.
- Done: `tools/check verb_registry` exits 0 (job check-41-turn: ctest 100%, WINDOWS ORACLE PASS static, ORACLE PASS host). Committed as f28286b + the trim_out test-fix commit; box on line 62 of vibes/41-action-registry.md ticked.

Result: exit 0; done; log: logs/41-action-registry-L62-a1-1792038157094.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/41-action-registry.md
```

## Task: wheel + transport play_stop (shared WheelState)

### Attempt 1 — 2026-10-15T04:29:21Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: prior subtask (turn verbs) committed (20f5ab1). wheel_mode.h defines `enum class WheelMode{Jog,Shtl,Scrl}`; Router::OnButton (router.cpp:17-47): jog/scrl hand-off ApplyShuttleRate(1.0) only from Shtl; shtl press sets shuttle_prev when not already Shtl; play_stop in Shtl = HardStop + Set(shuttle_prev), else RunCommand(40044).
- Facts: done — `WheelState` struct + public `wheel_state` member + deleted copy ctor in verb_registry.h; WheelVerb/TransportPlayStopVerb + `wheel`/`transport` press specs in verb_registry.cpp (factories capture `&reg.wheel_state`); kStopPlayCommand=40044 at file scope; tests extended (14 names, press-only gesture checks, press bad-args, new test_wheel_transport_behaviors); FakeSink records shuttle_rates.
- Done: `tools/check verb_registry` PASS (host + Windows oracle); line 69 ticked; committed 57b6bfc. (Fix along the way: deleting the copy ctor also suppressed the implicit default+move ctors — added explicit `= default` for both.)

Result: exit 0; done; log: logs/41-action-registry-L69-a1-1792038561818.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/41-action-registry.md
```

## Task: 2. Bindings parser (pure)

### Attempt 1 — 2026-10-15T04:35:47Z — budget 1800s

- Facts: `action`/`targeted` factories only accepted numeric ids (ParseIntArg); `verb_registry_test.cpp:145` asserts `action abc` is rejected. Design says `<id|_NamedCmd>` — so accept only a `_`-prefixed named form; bare `abc` stays an error (existing test kept green).
- Facts: named-command execution needs a sink seam: added non-pure `IActionSink::RunCommandByName(const std::string&)` (default no-op) so existing fakes in router/verbs/session tests don't break; REAPER impl (NamedCommandLookup) lands in task 4.
- Facts: key names = `KEY_*` suffixes from src/speed_editor/key_map.h (STOP/PLAY → `STOP_PLAY`); tests can `#include "speed_editor/key_map.h"` (REPO_ROOT/src is on the test include path).
- Dead ends: `out.bindings[{key, gesture}] = BoundVerb{...}` won't compile — `BoundVerb` is non-copyable (unique_ptr member), so map::operator[] + copy-assign hits the deleted copy. Fix: `BoundVerb` gets explicit defaulted move / deleted copy ctors and the parser uses `insert_or_assign(key, std::move(bound))`.
- Facts: `nudge` factory takes exactly one arg until task 7 adds `step=` — the design's example line `nudge trim_in step=frame` must not appear in task-2 test fixtures (would be a parse error today).
- Dead ends: parser must Trim inside `KEY . gesture` (whitespace-tolerant means `SPLIT   . tap` parses); `std::move(built.verb)` with a `const` `built` still copies (const rvalue) — keep the Build result non-const.
- Done: plugin/src/bindings.{h,cpp} (Tuning, BoundVerb, Bindings, ParseBindings, KeyByName/KeyName; sections [tuning]/[bindings], `;`/`#` comments, press-exclusivity, `bindings.ini:<n>: <reason>` errors, derived double_tap_keys); plugin/test/bindings_test.cpp (12 tests incl. the design's example file — minus `step=frame`, see above); action/targeted accept `_named` commands via new non-pure IActionSink::RunCommandByName (default no-op; REAPER impl = task 4); src/bindings.cpp added to PLUGIN_PURE_SOURCES. `SKIP_WINDOWS=1 tools/check bindings` PASS + all 19 plugin suites pass. Line 86 ticked; committed 667d8d3; full `tools/check bindings` (incl. Windows oracle) PASS rc=0 (job 41-bindings-final).

Result: exit 0; done; log: logs/41-action-registry-L86-a1-1792038947156.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/41-action-registry.md
```

## Task: 3. Default bindings = today's behavior

### Attempt 2 — 2026-10-15T04:49:37Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts: current tables → verb mapping established: press JOG/SCRL/SHTL = wheel jog/scrl/shtl, STOP_PLAY = transport play_stop; kVerbTable id rows → `action <id>` (SPLIT 40012, IN 40625, OUT 40626, IN/OUT/ESC double-tap/tap 40635, ESC double 40029, AUDIO_LEVEL double 40157, FULL_VIEW 40346); CompositeRows → targeted 40511/40512 (TRIM_IN/OUT), ripple_delete (RIPL_DEL), select_cursor_item (TIMELINE tap), action 40289 (TIMELINE double), take 1..9 (CAM1..9), volume_readout, smooth_cut, place_on_top, isolate_tracks, solo_tracks; kHoldRows → nudge move/trim_in/trim_out/slip_src/slip_dest/volume; TIMELINE turn = select_step. Total 40 (key,gesture) bindings; derived double set = IN, OUT, ESC, TIMELINE, AUDIO_LEVEL.
- Facts: test CMakeLists globs plugin/test/*_test.cpp automatically — no test CMake edit needed. Generated header goes to ${CMAKE_CURRENT_BINARY_DIR} (= build/plugin); test side can add ${CMAKE_BINARY_DIR} as include dir.
- Next: write plugin/src/default_bindings.ini, CMake file(READ) → generated header, plugin/test/default_bindings_test.cpp, then tools/check default_bindings.
- Dead ends: first generated header looked fine but failed to compile with a confusing "expected ',' or ';' before 'constexpr'" in key_map.h — the real cause was a missing `;` after the C string literal in the generated header (CMake file(WRITE) line lacked it). Verified by minimal repros in /tmp. CMake string(REPLACE) escaping for `\` → `\\`, `"` → `\"`, newline → `\n` all work as expected.
- Next: reconfigure, build, ctest -R default_bindings, then full tools/check default_bindings (bgrun, Windows oracle + host layer).
- Facts: unit test `default_bindings_test` passes (zero parse errors; derived double-tap set = {IN, OUT, TIMELINE, AUDIO_LEVEL, ESC} by key code; all 40 (key,gesture) rows match today's tables). Windows oracle `nix build .#reaper-plugin-windows` packs only tracked files → the untracked default_bindings.ini broke file(READ) at configure; committed the files, re-running full tools/check.
- Next: tools/check default_bindings must exit 0; if green, done.
- Done: tools/check default_bindings exits 0 (ctest default_bindings_test pass, WINDOWS ORACLE PASS static, ORACLE PASS host). Committed dc6d5a7.

Result: exit 0; done; log: logs/41-action-registry-L98-a2-1792039777686.log

### Done — 2026-10-15 — 2 attempt(s)
Files:
```
M vibes/.ralph/41-action-registry.md
```

## Task: 4. Switch over and delete the old tables

### Attempt 3 — 2026-10-15T05:06:05Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; 1 file changed, 5 insertions(+); log: logs/41-action-registry-L111-a3-1792040765654.log

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 3 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Log: logs/41-action-registry-L111-replan-1792041052564.log

## Task: 5. Actions: reload, open, show, echo

### Attempt 1 — 2026-10-15T17:00:11Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; 1 file changed, 5 insertions(+); log: logs/41-action-registry-L127-a1-1792083611750.log

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 1 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Log: logs/41-action-registry-L127-replan-1792083921938.log
