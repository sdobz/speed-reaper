# Ralph record: 45-cam-n

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- REAPER Lua export quirks: no `AddEmptyItem`/`SetEditCursor`/`GetEditCursor`; use `AddMediaItemToTrack`+`AddTakeToMediaItem`, `SetEditCurPos(pos,false,false)`, `GetCursorPosition()`.
- Native REAPER has NO per-track "select item at edit cursor" action; mouse-cursor variants (40528/40529/40530) are no-ops headless.
- `B_UISEL` set/clear per item is isolated (no cross-track effect) and does not move the edit cursor (cam_probe, REAPER 7.81).
- FakeProject (plugin/test/fake_project.h) only *records* `SetSelectedItems`, never applies it — selection-toggling tests must re-apply the last recorded call to `items[].selected` between calls.
- Real REAPER 7.81: `B_UISEL=1` alone makes `IsMediaItemSelected` true (B_SELECTED stays 0); `SetMediaItemSelected(false)` clears B_UISEL too. The plugin's `ItemInfo.selected` reads `IsMediaItemSelected`, so Lua B_UISEL toggles drive the CAM/CLOSE_UP select_track LEDs.
- User bindings.ini REPLACES the embedded defaults on load/reload (no merge); the plugin seeds the default file at first load, so a scenario that re-binds one key must rewrite the seeded file (swap the line) and restore it, not write a 1-line file.
- Adding a verb to the registry requires three follow-ups or `tools/check` fails: regenerate `docs/bindings.md` (gen_bindings_doc needs a built `build/plugin/verb_list`) and update `plugin/test/verb_registry_test.cpp`'s `kAllNames` count/list.
- A filtered `tools/check '<regex>'` run can hide regressions elsewhere in the suite (task 4's rebind left `led_read_test`/`verbs_test` red until task 7 ran the full ctest). Change default bindings → run the unfiltered suite.


## Task: 1. Research: native candidates → `docs/notes/cam-n-select.md`

### Attempt 1 — 2026-10-21T01:27:32Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `oracle/scenarios/cam_probe.lua` added (not in REAPER_SCENARIOS); `tools/reaper-oracle --layer reaper --scenario cam_probe` passes on REAPER 7.81.
- Measured: B_UISEL set/clear on track-2 item leaves track-1 item's B_UISEL/B_SELECTED untouched; edit cursor stays at 1.0 (Decisions confirmed, no contradiction).
- Candidates: 40528/40529/40530 (mouse-cursor based) are no-ops headless; 40416 works headless but is exclusive select AND moves the cursor (1.0→0.0); 40417 no-op at cursor 1.0.
- No native per-track toggle exists → H1 = no, no alternative id. H2: cursor-at-item-end for a named track not probeable headless; step-2 unit test is the arbiter.
- Candidate table + measured facts + `## Holes` appended to `docs/notes/cam-n-select.md`.

Dead ends:
- `reaper.AddEmptyItem`, `reaper.SetEditCursor`, `reaper.GetEditCursor` are nil in this REAPER 7.81 Lua export ("attempt to call a nil value") — replaced with `AddMediaItemToTrack`+`AddTakeToMediaItem` / `SetEditCurPos` / `GetCursorPosition`.

Next:
- Task 2 (`CursorItemOnTrack`) can start; it is the arbiter for H2.

Result: exit 0; done; log: logs/45-cam-n-L30-a1-1792546052389.log

### Done — 2026-10-21 — 1 attempt(s)
Files:
```
M vibes/.ralph/45-cam-n.md
```

## Task: 2. `CursorItemOnTrack`

### Attempt 2 — 2026-10-21T01:32:04Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `CursorItemOnTrack(items, track, cursor)` added to `plugin/src/target.{h,cpp}`; identical containment rule extracted from `CursorTargetItem`, which now reuses it (`-1` → empty).
- New `TestCursorItemOnTrack` in `plugin/test/target_test.cpp` covers inside, boundary → starter, last-item end, before/after → -1, other-track item ignored. `tools/check target` exit 0.

Dead ends:
- First draft of the new test asserted `on(1, 0.75) == -1` although cursor 0.75 is inside track-1 item 7 (0.25–1.25); the check, not the code, was wrong. Fixed the assertion.

Next:
- Task 3: `select_track` verb + `state_read` provider (use `CursorItemOnTrack`).

Result: exit 0; done; log: logs/45-cam-n-L42-a2-1792546324393.log

### Done — 2026-10-21 — 2 attempt(s)
Files:
```
M vibes/.ralph/45-cam-n.md
```

## Task: 3. Verb `select_track` + its LED provider

### Attempt 3 — 2026-10-21T01:34:52Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `SelectTrackTap(project, n)` + `SelectTrackVerb` added to `plugin/src/verb_registry.cpp`; registered `select_track` {Tap}, `ParseIntArg` name `select_track`, range error `select_track: N must be 1..9`; `state_read` = CursorItemOnTrack(n-1) exists and selected, nullopt on parse/range error.
- `plugin/test/select_track_test.cpp` added (CMake GLOBs `*_test.cpp` — no CMakeLists edit needed; ctest name `select_track`).
- `tools/check` forces `docs/bindings.md` in sync via `tools/gen_bindings_doc` — regenerate after building (`build/plugin/verb_list`), else check fails before ctest.
- `plugin/test/verb_registry_test.cpp` asserts the full verb list: updated to 15 names incl. `select_track` (args `{"1"}` in resolve loop); tap-only gesture loop covers it as-is.
- No cmake on PATH — build via `nix develop -c bash -c ...`.

Dead ends:
- First build failed: `res.verb->Run({sink, p})` — cannot bind rvalue brace-init to `VerbContext&`; use a named `VerbContext ctx{sink, p}`. (Also: piping the build through `tail` masked the error — check exit codes.)

Facts (cont.):
- `select_track_test` and `verb_registry_test` both pass; `docs/bindings.md` regenerated (+1 row: `select_track | tap | <N>`).

Next:
- DONE: job `check1` (`tools/check select_track`) rc=0 — ctest select_track pass, docs/bindings.md in sync, Windows static cross-build pass, oracle host pass. Line 54 ticked.

Result: exit 0; done; log: logs/45-cam-n-L54-a3-1792546492264.log

### Done — 2026-10-21 — 3 attempt(s)
Files:
```
M vibes/.ralph/45-cam-n.md
```

## Task: 4. Default bindings: CAM1..9 → `select_track N`

### Attempt 4 — 2026-10-21T01:42:23Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `default_bindings.ini`: nine `take N` lines → `select_track N`, plus a comment that any LED-bearing key may be bound to select_track N (its LED then follows track N).
- `default_bindings_test.cpp` kExpected: CAM1..9 → `select_track`.
- `led_state_test.cpp`: new block incl. `default_bindings_generated.h` — real defaults (kDefaultBindingsIni) with track-2 item under cursor selected → only CAM2 bit (snap/ripple/trim/isolate/solo all off in the fake, so no other default binding lights);
  CAM1 → select_track 3 follows track 3; CLOSE_UP → select_track 2 lights LED_CLOSE_UP.
- `docs/bindings.md` embeds the default file at the bottom (line ~172) → must regenerate after the build (job `check4` running: `tools/check "default_bindings|led_state"`).
- `gen_bindings_doc` works directly (no nix wrapper needed) once `build/plugin/verb_list` exists; regenerated docs/bindings.md.

Next:
- DONE: job `check4b` (`tools/check "default_bindings|led_state"`) rc=0 — default_bindings_test + led_state_test pass, docs in sync, Windows cross-build + oracle host pass. Line 66 ticked.

Result: exit 0; done; log: logs/45-cam-n-L66-a4-1792546943546.log

### Done — 2026-10-21 — 4 attempt(s)
Files:
```
M vibes/.ralph/45-cam-n.md
```

## Task: 5. Oracle: `cam_select` replaces `leds_cam`

### Attempt 5 — 2026-10-21T01:47:09Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Verified with a throwaway REAPER probe (REAPER 7.81): `SetMediaItemInfo_Value(it,"B_UISEL",1)` alone makes `IsMediaItemSelected(it)` true (B_SELECTED stays 0) — the plugin's `ItemInfo.selected` reads `IsMediaItemSelected`, so B_UISEL drives the CAM LED; `SetMediaItemSelected(false)` clears B_UISEL too (tap-deselect fully darkens the bit); new items from `AddMediaItemToTrack` start unselected.
- Tap gestures are deferred `double_tap_ms` (300 ms) after key-up, so the verb lands ~300 ms after the tap; a 1.5 s assert window from the device timestamp absorbs it.
- Device clock ≈ script clock once `start("device")` syncs on `log connected`; offset is calibrated as in leds_cam.lua.
- Wrote `oracle/fake/cam_select.txt` (two CAM2 0x34 taps at 2700/2750 and 4200/4250), `oracle/scenarios/cam_select.{env,lua}` (phase 1: default bindings, B_UISEL/tap/cursor steps; phase 2: written bindings.ini `CLOSE_UP.tap = select_track 2` + `_SPEED_REAPER_RELOAD_BINDINGS`), deleted `leds_cam.{txt,lua,env}`, swapped `leds_cam` → `cam_select` in tools/reaper-oracle REAPER_SCENARIOS.
- `tools/reaper-oracle --layer reaper --scenario cam_select`: ORACLE PASS, 20/20 checks, wall ~18.5 s (under the 20 s scenario timeout; final wait trimmed 13500→13200 ms for margin).
- `docs/notes/led-binding.md:99` still mentions `leds_cam` in a historical sentence (out of task scope; left as-is).

Dead ends:
- None (one harness flake: a probe scenario run exited early with no probe.json; a plain re-run of `load` and the manual re-run worked — transient, not reproducible).

Next:
- Task done: line 81 ticked, committed.

Result: exit 0; done; log: logs/45-cam-n-L81-a5-1792547229451.log

### Done — 2026-10-21 — 5 attempt(s)
Files:
```
M vibes/.ralph/45-cam-n.md
```

## Task: 6. Keep `take` e2e coverage in `simple_verbs`

### Attempt 6 — 2026-10-21T02:03:25Z — budget 1800s
- Facts: default_bindings.ini now has `CAM2.tap = select_track 2` (CAM1–9 → select_track N), so the fake device's CAM 2 tap no longer runs `take 2`. `take` is a registry verb (plugin/src/verb_registry.cpp:696), not a command — emits no `cmd` trace line, so the scenario's cmd/gesture trace checks are unaffected.
- Facts: mirrored oracle/scenarios/bindings.lua — inside start("device"), before the first device event (fake device first tap ~400 ms device time, well after setup): write `<ResourcePath>/speed-reaper/bindings.ini` with `CAM2.tap = take 2`, run `_SPEED_REAPER_RELOAD_BINDINGS` via NamedCommandLookup+Main_OnCommand, `os.remove` the file on the finish path. Header comment updated to explain the override.
- Facts: user bindings.ini is merged over the embedded defaults (bindings.lua scenario relies on this: only 3 lines written, other keys still bound).
- Dead ends: writing a 1-line bindings.ini (`CAM2.tap = take 2` only) + reload — the user file REPLACES the embedded defaults on reload (verbs_.Reload parses the text as the complete set; trace showed `bindings loaded n=1`), which unbound AUDIO_LEVEL.double (degraded to two taps) and FULL_VIEW. Scenario then failed on marker/trace checks.
- Facts: the plugin seeds `<ResourcePath>/speed-reaper/bindings.ini` with the defaults at first load (trace line 0 `bindings written ...`), and the reload action reads that same path (plugin/src/main.cpp:65 BindingsPath). So the scenario now: reads the seeded file, drops every `CAM2.*` line, appends `CAM2.tap = take 2`, writes back, reloads (trace `bindings loaded n=48`, full default set + the CAM2 override), and restores the original file text on finish.
- Done: job `sv2` → `ORACLE PASS reaper/simple_verbs` (trace: `echo CAM2.tap → take 2`, markers/40157/40346 all green); seeded bindings.ini verified restored to defaults. Line 91 ticked.

Result: exit 0; done; 3 files changed, 58 insertions(+), 3 deletions(-); log: logs/45-cam-n-L91-a6-1792548205523.log

### Done — 2026-10-21 — 6 attempt(s)
Files:
```
M oracle/scenarios/simple_verbs.lua
 M vibes/.ralph/45-cam-n.md
 M vibes/45-cam-n.md
```

## Task: 7. Docs: design + README

### Attempt 7 — 2026-10-21T02:08:42Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Verb + defaults already in place (tasks 3–4): `select_track` registered with state read (item on track N under cursor is selected), `docs/bindings.md` already regenerated and stale-check passes.
- Edits made: `docs/design.md` §6 new `select_track` row; §10 "Other seeded reads" rewritten — CAM keys default `select_track <n>`, LED lights while that item is selected (unpressed); `take <N>` described as the read for any key bound to `take N`. `README.md` CAM row → "item under the cursor on track N is selected".
- `docs/notes/led-binding.md` line 87 still says CAM = `take N` read but is annotated "unchanged here (45 rebinds)" — left as-is (out of this task's scope).

Facts (cont.):
- HEAD was RED: `led_read_test` and `verbs_test` still asserted the pre-task-4 CAM=`take N` defaults (task 4 only ran a ctest filter, so this slipped through). Fixed: led_read_test's two CAM blocks now expect `select_track` behavior (mask = CAM1|CAM2 for the two under-cursor selected items, 0 once deselected; the `take N` read stays covered in led_state_test); verbs_test `test_cam_takes` now drives the `take` verb via an explicit `CAM1/2.tap = take 1/2` bindings string (mirrors the simple_verbs oracle scenario).
- In led_read_test both items are under the cursor on separate tracks and selected → mask is `Bit(LED_CAM1)|Bit(LED_CAM2)`, not just CAM1 (first draft was wrong).

Dead ends:
- Expecting mask == Bit(LED_CAM1) in led_read_test — the track-2 item is also under the cursor and selected, so CAM2 is lit too.

Next:
- Full `tools/check` bg job `check9` running (incl. windows + oracle host); then the two greps, tick line 102, commit.
