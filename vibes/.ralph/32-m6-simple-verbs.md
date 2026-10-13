# Ralph record: 32-m6-simple-verbs

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- REAPER-API rule (main.cpp comment): every new API a milestone adds needs `REAPERAPI_WANT_<name>` in BOTH plugin/src/reaper_project.cpp (or the calling TU) AND main.cpp's list (owns REAPERAPI_IMPLEMENT), or the plugin refuses to load.
- The host-layer oracle (plugin_host) resolves GetFunc for any name in oracle/reaper_api_names.txt (REAPER 7.81 export list, 874 names); all other names return nullptr.
- `MediaItem` is only forward-declared in the vendored SDK (reaper_plugin.h:1465) — you cannot dereference item fields; use the string APIs (GetMediaItemInfo_Value / SetMediaItemInfo_Value, e.g. "I_CURTAKE").
- REAPER 7.x Lua quirks (verified, see docs/notes/m5-observations.md ## M6): track name param is `P_NAME` ("NAME" silently fails both directions); `GetSetMediaTrackInfo_String` is 4-arg (no buf_len); `EnumProjectMarkers` returns (retval, isrgn, pos, rgnend, name, markrgnindexnumber) 0-based, idx 0's first return is retval not count — count with `GetNumRegionsOrMarkers(nil)`; `InsertTrackAtIndex` returns nothing in Lua (use `GetTrack`); `MediaTrack`/`ProjectMarker` are userdata (no `.name` field).
- tools/check needs the nix dev shell for cmake (`nix develop --command ...`); it runs: make -C src, plugin build with -Werror + SPEED_REAPER_FAKE_DEVICE=ON, nm export check, ctest, Windows cross build (tools/windows-oracle --layer static), oracle host layer. Set SKIP_WINDOWS=1 / ORACLE_FULL for variants.
- "Above" in track order = SMALLER index (track 0 is the arrange top). `InsertTrackInProject(nullptr, idx, flags)` (void) inserts a new track at idx; source track at idx shifts to idx+1. Inserting into several tracks in one pass: process indices DESCENDING so smaller pending indices stay valid. `GetItemStateChunk(item, buf, sz, bool)` (bool ret, C string) / `SetItemStateChunk(item, str, bool)` copy an item's state; `AddMediaItemToTrack(track)` creates the empty item.
- REAPER crossfade item actions (measured headless, tone sources): `40916` "Crossfade items within time selection" creates auto-fades ONLY at item boundaries (meeting pairs, or lane overlap starts) that lie strictly INSIDE the time selection; overlapping items that span the selection with no internal boundary are untouched (40916 AND `41059` no-op). Selection state does not matter. These are the only two crossfade-CREATING item actions in docs/reaper-actions.tsv. There is no "split items at time selection" action. Item selection can only be set via commands in this Lua build (40717 select-in-tsel; 40289/40297/40769 deselect); `B_SELECTED` writes are no-ops.


## Task: 1. Extend `IProject` for takes

### Attempt 1 — 2026-10-13T06:20:29Z — budget 1800s

Facts:
- IProject has two implementers only: ReaperProject (plugin/src/reaper_project.{h,cpp}) and FakeProject (plugin/test/fake_project.h).
- Item id == index in EnumerateItems() order (track, then pos) — see reaper_project.cpp; all id-taking methods enumerate per call (SetSelectedItems does too).
- Take APIs used: CountTakes (count), GetMediaItemInfo_Value(item, "I_CURTAKE") (0-based active take), SetMediaItemInfo_Value(item, "I_CURTAKE", n) + UpdateItemInProject (set). All three are in oracle/reaper_api_names.txt, so the host oracle resolves them.
- Contract added: missing id → ItemTakeCount 0, ItemActiveTake -1, SetItemActiveTake no-op. Fake: take_counts/active_takes index-aligned with items (missing entry = 1-take item); set_take_calls records (id,take) and applies for valid ids (unlike selected_calls which is record-only).
- Local plugin build + all 11 ctest tests pass with -Werror.
- Job `check` running tools/check in background (vibes/.ralph/logs/check.log).

Dead ends:
- None yet. (First draft put ItemById before EnumerateItems in the anon namespace — use-before-declaration; moved it after.)

Next:
- DONE: job `check` exited 0 (11/11 tests, WINDOWS ORACLE PASS static, ORACLE PASS host); line 15 ticked.

Result: exit 0; done; log: logs/32-m6-simple-verbs-L15-a1-1791872429886.log

### Done — 2026-10-13 — 1 attempt(s)
Files:
```
M vibes/.ralph/32-m6-simple-verbs.md
```

## Task: 2. CAM 1–9 = active take N

### Attempt 2 — 2026-10-13T06:28:22Z — budget 1800s

Facts:
- Design §3 CAM row: "T: set active take N (I_CURTAKE = N-1, clamped to CountTakes) on each target item; items with fewer takes keep theirs". Implemented as IProject APIs (no REAPER command id): one free fn `CamSetTake` in verbs.cpp derives N from the key (KEY_CAM1..KEY_CAM9 are contiguous 0x0033..0x003B); no target → Trace("target none"), else Undo_Begin + WithTargetSelected + per-target `if (ItemTakeCount(id) >= n) SetItemActiveTake(id, n-1)` + Undo_End("Speed Reaper: take N").
- Composite table is now a function-local static `std::vector<CompositeRow> CompositeRows()` (built at first use): 6 M5 rows + a loop over n=1..9 for the CAM rows. CompositeFor iterates it; behavior otherwise unchanged.
- Tests added in verbs_test.cpp::test_cam_takes: (1) CAM2, two selected targets with take_counts {3,1} → set_take_calls==[(0,1)], B untouched, label "Speed Reaper: take 2", selection restore only; (2) CAM1, no target → nothing (no take calls, no undo, no selection churn).
- No REAPER API added in this task (ItemTakeCount/ItemActiveTake/SetItemActiveTake exist from task 1), so no REAPERAPI_WANT changes needed.
- Job `check2`: tools/check verbs (nix dev shell).

Dead ends:
- None. (No cmake on PATH outside the nix shell — go straight to tools/check via bgrun.)

Next:
- DONE: job `check2` exited 0 (verbs_test passed, -Werror clean, WINDOWS ORACLE PASS static, ORACLE PASS host); line 28 ticked.

Result: exit 0; done; log: logs/32-m6-simple-verbs-L28-a2-1791872902185.log

### Done — 2026-10-13 — 2 attempt(s)
Files:
```
M vibes/.ralph/32-m6-simple-verbs.md
```

## Task: 3. MARK, level read-out, SMTH CUT

### Attempt 3 — 2026-10-13T06:31:55Z — budget 1800s

Facts:
- Three rows implemented in plugin/src/verbs.cpp: (a) kVerbTable row {KEY_AUDIO_LEVEL, DoubleTap, 40157} (Insert marker at edit cursor) + KEY_AUDIO_LEVEL appended to VerbDoubleTapKeys(); (b) composite AudioLevelTap: TargetItems(project).front(), message `Volume: %.1f dB` of 20*log10(project.ItemVolume(id)) via new IActionSink::Message (REAPER: ShowConsoleMsg, already in main.cpp WANT list); no targets → Trace("target none"); pure read-out, no selection churn/undo; (c) composite SmthCutTap: HasTimeSelection → RunCommand(40916), else Trace("no time selection").
- IProject::ItemVolume(id) = GetMediaItemInfo_Value(item, "D_VOL") (linear; 1.0 = 0 dB), out-of-range id → 0.0; no new REAPER API needed (GetMediaItemInfo_Value already WANTED in both TUs).
- FakeProject: item_volumes vector index-aligned (missing entry = 1.0, out-of-range = 0.0), mirroring the take arrays.
- IActionSink gained pure virtual Message(const char*); implementers touched: ReaperActionSink (main.cpp: Trace + ShowConsoleMsg), FakeSinks in router_test.cpp and verbs_test.cpp (verbs one records messages).
- Tests: verbs_test.cpp — 40157 row in test_mapped_gestures, VerbDoubleTapKeys size 5, new test_audio_level_and_smth_cut (D_VOL 0.5 → "Volume: -6.0 dB"; selection-first target → "Volume: 0.0 dB"; no target → silent; SMTH CUT with/without time sel).
- Local: plugin -Werror build clean, verbs_test passes 1/1.

Dead ends:
- First build failed: ItemVolume declared in reaper_project.h was forgotten (ReaperProject final → pure-virtual error). One-line fix.

Next:
- DONE: job `m6-verify` exited 0 (verbs_test passed, -Werror clean, WINDOWS ORACLE PASS static, ORACLE PASS host); line 40 ticked.

Result: exit 0; done; 11 files changed, 182 insertions(+), 4 deletions(-); log: logs/32-m6-simple-verbs-L40-a3-1791873115527.log

### Done — 2026-10-13 — 3 attempt(s)
Files:
```
M plugin/src/action_sink.h
 M plugin/src/main.cpp
 M plugin/src/project.h
 M plugin/src/reaper_project.cpp
 M plugin/src/reaper_project.h
 M plugin/src/verbs.cpp
 M plugin/test/fake_project.h
 M plugin/test/router_test.cpp
 M plugin/test/verbs_test.cpp
 M vibes/.ralph/32-m6-simple-verbs.md
 M vibes/32-m6-simple-verbs.md
```

## Task: 4. PLACE ON TOP

### Attempt 4 — 2026-10-13T06:36:30Z — budget 1800s

Facts:
- IProject::DuplicateItemsAbove(const std::vector<int>&) added (project.h, ReaperProject, FakeProject). ReaperProject: unique source tracks (ids→track index), processed DESCENDING so InsertTrackInProject(nullptr, t, 0) (inserts at index t, i.e. above src) never invalidates still-pending smaller indices; copies via AddMediaItemToTrack + GetItemStateChunk→SetItemStateChunk + D_POSITION re-set + UpdateItemInProject; originals/selection untouched.
- Vendored SDK signatures (vendor/reaper-sdk/sdk/reaper_plugin_functions.h): InsertTrackInProject(ReaProject*, int idx, int flags) void; AddMediaItemToTrack(MediaTrack*) → MediaItem*; GetItemStateChunk(MediaItem*, char* buf, int sz, bool) → bool; SetItemStateChunk(MediaItem*, const char*, bool) → bool. All four names (plus UpdateItemInProject) are in oracle/reaper_api_names.txt, so the host oracle resolves them.
- REAPERAPI_WANT added for the four new names in BOTH reaper_project.cpp and main.cpp (REAPERAPI rule).
- Composite row: PlaceOnTopTap in verbs.cpp — TargetItems, empty → Trace("target none"); else Undo_Begin + WithTargetSelected { project.DuplicateItemsAbove(targets) } + Undo_End("Speed Reaper: place on top"). Row {KEY_PLACE_ON_TOP, Tap} appended in CompositeRows().
- FakeProject: duplicate_calls records ids; also SIMULATES (descending source tracks: items on tracks above shift +1, copies appended on the new track at index t, same pos, unselected, ids renumbered) — stronger than record-only.
- Tests: verbs_test.cpp test_place_on_top — (1) cursor target id 0 on track 0 → one call [0], label, 2 selection calls (select then restore empty), items[1] on track 1 pos 0.0 unselected; (2) no target → nothing.
- Oracle host scenarios don't tap PLACE ON TOP (checked oracle/scenarios/verbs.lua), so no host-layer change.
- Job `build`: nix develop cmake build + ctest -R verbs (vibes/.ralph/logs/build.log).

Dead ends:
- InsertTrackChain is NOT in the vendored SDK / oracle names list; InsertTrackInProject is — used that instead.
- First fake draft shifted only tracks > t, then wrote copies at track t; my first test expectation also had it backwards (copy on track 1, original on 0). Correct semantics: insert at idx t puts the NEW track at index t (above, smaller index) and the source track shifts to t+1 — so original A ends on track 1, copy on track 0. Both fixed; all 11 tests pass.

Next:
- DONE: job `verify` exited 0 (verbs_test 1/1, -Werror clean, WINDOWS ORACLE PASS static, ORACLE PASS host); line 52 ticked.

Result: exit 0; done; log: logs/32-m6-simple-verbs-L52-a4-1791873390532.log

### Done — 2026-10-13 — 4 attempt(s)
Files:
```
M vibes/.ralph/32-m6-simple-verbs.md
```

## Task: 5. View buttons: isolate, solo, maximize

### Attempt 5 — 2026-10-13T06:43:41Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: prior attempts left no code (only this record header). Design §3 (docs/plugin-design-m5-m9.md:86-88): VIDEO_ONLY = isolate (hide all tracks except target items' tracks, B_SHOWINTCP/B_SHOWINMIXER, second tap restores), AUDIO_ONLY = solo (I_SOLO on target tracks, second tap restores), FULL VIEW = candidate 40346. SetMediaTrackInfo_Value IS in oracle/reaper_api_names.txt, so oracle-host layer links.
- Facts: implemented IProject::ToggleIsolateTracks/ToggleSoloTracks (project.h), FakeProject track_views{show_tcp,show_mixer,solo} + saved state (fake_project.h), ReaperProject with REAPERAPI_WANT_SetMediaTrackInfo_Value added to BOTH reaper_project.cpp and main.cpp, verb rows VIDEO_ONLY/AUDIO_ONLY (composites, no-target → trace `target none`, no undo block) + FULL_VIEW id row 40346 in kVerbTable, test_view_buttons in verbs_test.cpp (direct fake toggles with target ids + routing + restore-exactly checks).
- Facts: no-target toggle call (bad ids) is a recorded no-op that does NOT flip active state, in both fake and Reaper.
- Done: `tools/check verbs` passed rc=0 (job `m6task5`, log vibes/.ralph/logs/m6task5.log) — ctest verbs_test green, Windows cross build + oracle host layer pass. Line 64 ticked, committed (addd99e).
- Next (task 6): the oracle scenario can probe the toggles via the plugin; the saved isolate/solo state is ReaperProject member state, so a second gesture within one plugin lifetime exercises the restore path.

Result: exit 0; done; 1 file changed, 2 insertions(+), 1 deletion(-); log: logs/32-m6-simple-verbs-L64-a5-1791873821225.log

### Done — 2026-10-13 — 5 attempt(s)
Files:
```
M vibes/.ralph/32-m6-simple-verbs.md
```

## Task: 6. Oracle scenario: `simple_verbs`

### Attempt 6 — 2026-10-13T06:50:43Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: scenario files written: oracle/fake/simple_verbs.txt (CAM2 0x34 @400, AUDL 0x2C x2 @800/950, PLACE ON TOP 0x05 @1400, SMTH CUT 0x11 @1900, AUDIO_ONLY 0x26 x2 @2300/2550, VIDEO_ONLY 0x25 x2 @2950/3250, FULL VIEW 0x2D @3800) + oracle/scenarios/simple_verbs.env + simple_verbs.lua. Script clock = device - ~40 ms.
- Facts: per reaper-nvim action list (fetched): 40346 = "Toggle fullscreen" (exists, toggleable), 40916 = "Item: Crossfade items within time selection" (= design's SMTH CUT), 40157 = "Markers: Insert marker at current position". No table fixes expected; will confirm by running.
- Facts: device order has PLACE ON TOP BEFORE SMTH CUT on purpose — the [1,2] time sel must overlap A and C (A's copy) at once. Time sel [1,2] is set by the lua script at ~1600 ms (no Speed Editor button sets an arbitrary range).
- Facts: setup names the tracks "A"/"B" (SetMediaTrackInfo_String "NAME") so the assertions find A's/B's track after PLACE ON TOP shifts indices (the new copy track is unnamed). A gets a 2nd take via AddEmptyTake(itemA, true). SetTrackLastTouched(trackA) makes A the implicit target with no selection.
- Facts: no plugin source changes this task, so `tools/reaper-oracle --layer reaper` uses the existing build/plugin .so (fresh from task 5's tools/check).

Result: exit 0; progress; 2 files changed, 14 insertions(+), 2 deletions(-); log: logs/32-m6-simple-verbs-L75-a6-1791874243614.log

### Replan (depth 0) — SPIKE → "Spike: settle the SMTH CUT command (does 40916 crossfade?) — findings to docs/notes/m5-observations.md" then finish parent
Trigger: 6 attempt(s), last: exit 0, progress
Why: Attempt 6 wrote all artifacts and got the scenario running (9/18 checks pass; FULL VIEW id 40346 confirmed), then hit one real open question — 40916 measured as a no-op on overlapping A/C, so it's unclear whether the SMTH CUT id is wrong or a config was untested — plus four small Lua-binding bugs whose diagnoses live only in the attempt log, not the record the next worker can see.
Rejected: SPLIT: subtask 2 would just be the auto finish step and still branch on the id finding. BUDGET: the blocker is a research question, not time; a longer run re-discovers the same quirks. REWRITE: task and Verify are fine. BLOCK: the id question is answerable empirically with the oracle and the action list.
Log: logs/32-m6-simple-verbs-L75-replan-1791875721550.log

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? REAPER names it "Item: Crossfade items within time selection" 

### Attempt 1 — 2026-10-13T07:32:32Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? REAPER names it "Item: Crossfade items within time selection"

### Attempt 1 — 2026-10-13T07:05:00Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts (probe run 1): configs A/D (MEETING items, same track, tsel over meeting point) and B/C (two [0,5] fully overlapping on two DIFFERENT tracks, tsel [1,2]) — BOTH 40916 and 41059 left every fade at 0 (finA/foutA=0, no crossfade). New items default fin/fout=0.01 (not auto).
- Facts (probe run 1): this Lua build also lacks reaper.GetSetEditCurPos (nil) — the marker/name probes crashed before running. Added set_cursor() fallback (SetEditCurPos / JumpToTime) + configs E/F = OVERLAPPING items on ONE track (e1 [0,3.5], e2 [1.5,5], tsel [1.5,3.5]) with 40916 and 41059; re-run as job probe_smth2.

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? REAPER names it "Item: Crossfade items within time selection" 

Result: timeout after 180s without tool use (monologuing/stalled); progress; 2 files changed, 148 insertions(+), 114 deletions(-); log: logs/32-m6-simple-verbs-L79-a1-1791876752764.log

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? REAPER names it "Item: Crossfade items within time selection" 

### Attempt 2 — 2026-10-13T07:44:23Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? REAPER names it "Item: Crossfade items within time selection" 

Result: timeout after 180s without tool use (monologuing/stalled); no-progress; 2 files changed, 157 insertions(+), 114 deletions(-); log: logs/32-m6-simple-verbs-L79-a2-1791877463094.log

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? REAPER names it "Item: Crossfade items within time selection" 

### Replan (depth 1) — REWRITE → "Spike: settle the SMTH CUT command (does 40916 crossfade? fallback candidate `41059`) — findings to docs/notes/m5-observations.md"
Trigger: 2 attempt(s), last: timeout after 180s without tool use (monologuing/stalled), no-progress
Why: Both spike attempts stalled 180s with zero tool use on a ~600-word dense body, while the tree already contains a finished probe_smth.lua (attempt 1 ran it and found 40916/41059 no-op on configs A–D); what's left is run one command, read one JSON, write three note bullets.
Rejected: SPLIT: the probe runs all three investigations in one script and the write-up is ~15 lines — small enough for one sitting; the failure was spec readability, not size. BUDGET: the stall was monologuing with no tool use, not time pressure, and the record shows no progress on this leaf. BLOCK: the id question is empirically answerable and mostly answered already; only configs E/F need running and transcribing.
Log: logs/32-m6-simple-verbs-L79-replan-1791877783391.log

## Task: Spike: settle the SMTH CUT command (does 40916 crossfade? fallback candidate `41059`) — findings to docs/notes/m5-observ

### Attempt 1 — 2026-10-13T07:55:30Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: probe_smth run 2 had already completed — build/oracle/probe_probe_smth.json has all six configs (A–F); 40916 AND 41059 leave every fade 0 (finA/foutA/xf/xfn) in all of them → verdict: no id works in A–F.
- Facts: probe_smth's marker probe was misleading: it queried EnumProjectMarkers(1) with only one marker present. Wrote throwaway follow-up oracle/scenarios/probe_mkr.lua (correct calls); run PASS, results in build/oracle/probe_probe_mkr.json.
- Facts (marker): EnumProjectMarkers Lua returns (retval, isrgn, pos, rgnend, name, markrgnindexnumber), 0-based idx. Marker at 1.0: EnumProjectMarkers(0) = 1|false|1.0|0.0|M1|1; idx 1 (out of range) = 0|false|0.0|0.0||0|nil|nil. So return #3 IS pos; attempt 6's i=1..n loop just queried out-of-range entries. EnumProjectMarkers(0)'s first return is retval (1=found), NOT a count; count with GetNumRegionsOrMarkers(nil). REAPER 7.82 reascripthelp confirms the 6-return signature (func is "discouraged", use GetNumRegionsOrMarkers/GetRegionOrMarker).
- Facts (track name): REAPER 7.x param is "P_NAME", not "NAME". 4-arg GetSetMediaTrackInfo_String(tr,"P_NAME","NAMETEST",true) → set=true; read-back (tr,"P_NAME","",false) = (true,"NAMETEST") — STICKS. "NAME" returns false + empty buf on both set and read (attempt 6's lookup failed for that reason). Lua binding is 4-arg, no buf_len; MediaTrack is userdata (no tr.name field). SetEditCurPos(1.0,false,false) works but returns nil; cursor read 1.0.
- Facts (SMTH CUT): docs/reaper-actions.tsv confirms 40916 = "Item: Crossfade items within time selection", 41059 = "Item: Crossfade any overlapping items" — both right actions; the no-op is likely because probe takes are empty (no audio source). Note written (## M6, three bullets).
- Dead ends: GetRegionOrMarkerInfo_Value/GetSetRegionOrMarkerInfo_String also take proj as arg #1 (nil); AddProjectMarker needs 6 args (wantidx, use -1); InsertTrackAtIndex returns nothing in Lua (use GetTrack after).
- Next: nothing — note written, box ticked. Parent task: expect the SMTH CUT check to need the plugin's own fade logic (no 40916/41059 config crossfaded empty-source items); name lookup in simple_verbs.lua must use "P_NAME" and 4-arg calls; count items per track by GetTrackMediaItem loop; marker pos = EnumProjectMarkers(0) return #3.

Result: exit 0; done; 3 files changed, 20 insertions(+), 1 deletion(-); log: logs/32-m6-simple-verbs-L79-a1-1791878130052.log

### Done — 2026-10-13 — 1 attempt(s)
Files:
```
M docs/notes/m5-observations.md
 M vibes/.ralph/32-m6-simple-verbs.md
 M vibes/32-m6-simple-verbs.md
?? oracle/scenarios/probe_mkr.lua
```

## Task: Finish the parent task using the findings of the step above

### Attempt 1 — 2026-10-13T08:10:36Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: M6 note applied to simple_verbs.lua: P_NAME 4-arg names; per-track item count via GetTrackMediaItem loop; markers via GetNumRegionsOrMarkers(nil) + 0-based EnumProjectMarkers (pos = return #3); want_cmds now {40157, 40916, 40346}; SMTH sample now tracks max of D_FADEINLEN_AUTO/D_FADEOUTLEN_AUTO/D_CROSSFADELEN/D_CROSSFADENEXT (fresh items: all 0).
- Facts: spike probe no-op had EMPTY takes; writing oracle/scenarios/probe_xf2.lua (throwaway) repeating configs S1-S6 with a real tone source (wav generated in Lua into $respath; SetMediaItemTakeInfo_String P_SOURCE). Running as bgrun `probe_xf2` — output build/oracle/probe_probe_xf2.json.
- Facts: THIS Lua build: `reaper.GetSet_LoopTimeRange` does NOT set (any bRead/bSel combo leaves loop/tsel 0..0); `reaper.GetSet_LoopTimeRange2(nil, isSet, bSetLoop, start, end, bSetPlay)` DOES set — isSet=true, bSetLoop=false sets the TIME SELECTION (read-back via isSet=false returns 1.0|2.0); bSetLoop=true reads/sets the loop. `GetProjectInfo_Value`/`SetProjectInfo_Value` don't exist; `GetSetProjectInfo(proj, name, value, bSet)` is 4-arg and setting D_TS_* is a no-op. `Main_OnCommand` returns no value here (tostring of the call errors with "no value"). PCM source attach: `PCM_Source_CreateFromFile(path)` + `SetMediaItemTake_Source` WORKS (read-back via GetMediaItemTake_Source/GetMediaSourceFileName); "P_SOURCE" string param is invalid (set=false); SetItemStateChunk with <PCM TAKE> chunk silently fails (set=false, item reset to [0,0]). simple_verbs.lua tsel set switched to GetSet_LoopTimeRange2.
- Facts: probe v6: 40916 AND 41059 still all-zero fades with REAL tone sources + properly set tsel + selected items (S1/S2/S3/S4). Probe v7 (running, job probe_xf2j..l history in vibes/.ralph/logs/) diffs item state chunks around 40916 + control action 40026 (trim to tsel) to see if item actions work at all + auto-crossfade option 40041 variant.
- Dead ends: GetSet_LoopTimeRange set; GetSetProjectInfo D_TS_* set; P_SOURCE string param; state-chunk PCM TAKE; item selection before 40916 (S4 still 0).
- Next: read probe_xf2 v7 JSON; if 40026 trims but 40916 no-ops even with source+tsel+chunk diff → crossfade actions are no-ops in headless/batch REAPER, the >0 fade check cannot pass → BLOCKED with evidence; if chunk diff shows crossfade state in fields we don't read → fix the fields measured in simple_verbs.lua.

Result: exit 0; progress; 2 files changed, 56 insertions(+), 21 deletions(-); log: logs/32-m6-simple-verbs-L88-a1-1791879036908.log

### Attempt 2 — 2026-10-13T08:39:30Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: CAM2 fixed by sampling I_CURTAKE IN-WINDOW (st<1350, track0/track1 item0) instead of reading post-SMTH-CUT items by name at assertion time. All 18 checks now pass EXCEPT SMTH CUT (job sv-run1: 17/18, only `SMTH CUT: D_FADEINLEN_AUTO > 0 after cut in [1,2]` fails, before=0 after=0). Marker at 1.0, item count, solo, hide/restore, all trace checks green.
- Facts: probe_sv2 (job sv-probe2): 40916 no-ops with tone sources + tsel [1,2] set+verified (read-back 1.0|2.0) + NO selection — all of D_FADEINLEN/AUTO, B_FADEIN_AUTO, D_CROSSFADELEN/NEXT stay at defaults (manual fades 0.01, rest 0). Marker phases M1-M3 all land at 1.0 (40157 fine in isolation; the earlier 0.0 was attempt-6-era).
- Facts: KVR 2018 thread: 40916 "Crossfade items within time selection" requires BOTH items selected. probe_sv3: `reaper.SelectMediaItemInProject` does NOT exist in this Lua build (nil), SetMediaItemInfo_Value(it,"B_SELECTED",1) write is a no-op (sel stays 0). Selection via command 40717 "Item: Select all items in current time selection" WORKS (sel_count=2) — but 40916 STILL no-ops with both items selected. Deselect: 40289 (items), 40297 (tracks), 40769 (both).
- Dead ends: 40916 with empty takes (M6 + v6), with tone sources, with tsel verified, with 40717 selection — all zero fades in fields D_FADEINLEN(_AUTO)/D_FADEOUTLEN(_AUTO)/B_FADEIN(_OUT)_AUTO/D_CROSSFADELEN/D_CROSSFADENEXT.
- Next: probe_sv4 (job sv-probe4, running) matrix: L2 41059 on [0,5]x2; L3 40916 items fully inside tsel [1,1.5]/[1.5,2]; L4 40916 meeting [0,2.5]/[2.5,5] tsel [1,4]; L5 40916 same-track lane overlap. If all zero → 40916/41059 cannot produce a measurable fade in this headless build → either record + BLOCKED, or (last resort) re-read task: check allows "the crossfade the build produces" — state-chunk diff would reveal non-field changes.

Result: exit 0; progress; 2 files changed, 84 insertions(+), 37 deletions(-); log: logs/32-m6-simple-verbs-L88-a2-1791880770193.log

### Attempt 3 — 2026-10-13T09:06:42Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: 40916 DOES crossfade — but only at item boundaries that ALREADY lie strictly inside the time selection. probe_sv4 (tone sources, 40717 selection): L3 meeting pair [1,1.5]+[1.5,2] tsel [1,2] → item1 D_FADEOUTLEN_AUTO=0.99, item2 D_FADEINLEN_AUTO=0.99; L4 meeting [0,2.5]+[2.5,5] tsel [1,4] → item2 D_FADEOUTLEN_AUTO=3.0; L5 same-track lanes [0,3]+[2,5] tsel [1,4] → item1 D_FADEOUTLEN_AUTO=3.0. 41059 on [0,5]×2 tsel [1,2] (L2) → all 0.
- Facts: the EXACT simple_verbs post-PLACE-ON-TOP layout cannot produce a fade. probe_sv5 = C [1.5,6.5] (start edge INSIDE tsel [1,2]) + A [0,5] + B [0,5], 40717 selection (sel_count=3), 40916 → every fade field 0 on all three items. Combined with probe_sv2 (40916, [0,5]×2, sources, no selection) and probe_sv3 (40916, [0,5]×2, 40717 selection) and probe_sv4 L2 (41059, [0,5]×2): overlapping items that span the selection with no internal boundary → 40916 and 41059 both no-op, with or without sources, with or without selection. REAPER action list has only these two crossfade-creating item actions (40916, 41059; all others are shape/length/default setters or Crossfade-Editor context actions).
- Facts: re-ran the current scenario (this attempt): 17/18, only `SMTH CUT: D_FADEINLEN_AUTO > 0 after cut in [1,2]` fails (before=0 after=0). All other checks green with the attempt-2/3 simple_verbs.lua (in-window I_CURTAKE sampling, P_NAME lookups, per-track item loop, 0-based EnumProjectMarkers pos=return#3, want_cmds {40157,40916,40346}).
- Facts: plugin verb matches the design exactly (docs/plugin-design-m5-m9.md: "SMTH CUT | Tap | crossfade within time selection (X) ... | 40916"); SmthCutTap runs sink.RunCommand(40916) iff HasTimeSelection(). PLACE ON TOP keeps the copy at the source position (reaper_project.cpp DuplicateItemsAbove), so C is [0,5] — no boundary in [1,2] by construction.
- Dead ends: 40916 on the prescribed layout — empty takes (M6 spike), tone sources (sv2), +selection (sv3), 3-track exact layout with C start inside tsel (sv5); 41059 on [0,5]×2 (sv4 L2). probe_xf2 state-chunk-diff probe and its probe_xf3 bisect (PROBE_STEP 1=40026, 2=GetItemStateChunk, 3=40041) all timed out (no probe.json within 60 s) — chunk-diffing is not viable in this headless env.
- Next: MARKED BLOCKED (line 88 `- [!]`). The >0 fade check contradicts REAPER's actual behavior for the prescribed setup: no item boundary exists inside [1,2] (A/B/C all [0,5]), and neither 40916 nor 41059 creates fades without one. Fixing needs a planner decision: (a) change the scenario setup so a boundary lies in [1,2] (e.g. B or C at a different start), keeping the check; or (b) change the SMTH CUT verb to split items at the tsel edges first — no "split items at time selection" action exists in REAPER, so that needs a new IProject API + rebuild, i.e. a design change, not the one-line id fix the finish step anticipated. M6 note bullet 1 in docs/notes/m5-observations.md updated with the corrected verdict.

Result: exit 0; worker-blocked; 4 files changed, 99 insertions(+), 39 deletions(-); log: logs/32-m6-simple-verbs-L88-a3-1791882402927.log

### Replan (depth 1) — REWRITE → "Make simple_verbs pass: put a meeting A/C boundary at 1.5 (inside tsel [1,2]) with tone sources — the proven 40916 pattern"
Trigger: worker marked it blocked: 17/18 checks pass, but the SMTH CUT >0-fade check contradicts REAPER's actual behavior for the prescribed setup — A/B/C are all [0,5] with tsel [1,2], so no item boundary lies inside the selection, and both crossfade-creating actions (40916, 41059) only fade at boundaries strictly inside the tsel (measured: 40916 works there, e.g. 0.99/0.99 auto-fades; no-op on the spanning-overlap layout in 6+ configs with real audio sources — see docs/notes/m5-observations.md ## M6 and vibes/.ralph/32-m6-simple-verbs.md). Needs a planner decision: change the scenario setup so a boundary lies in [1,2], or change the SMTH CUT verb to split items at the tsel edges first (no such REAPER action exists; new IProject API + rebuild required).
Why: The one failing check is a setup/behavior mismatch, now fully measured: 40916 (the correct id, per design) only crossfades at an item boundary strictly inside the tsel where an item ends (L3 0.99/0.99, L4 3.0 in probe_probe_sv4.json), and the prescribed all-[0,5] A/B/C layout has no such boundary (sv5 no-op) — so the scenario setup must change, not the plugin.
Rejected: BLOCK — the (a) setup vs (b) verb change is a planner call: (b) means a new IProject API + rebuild + product-design change while the check already accepts 'the crossfade the build produces'; (a) only edits our own scenario. SPIKE — all needed measurements already exist in ## M6 and the probe JSONs. SPLIT — it is one file change plus one text line, splitting just adds a 30-min turn. BUDGET — prior attempts finished in time; the blocker was the setup, not duration.
Log: logs/32-m6-simple-verbs-L88-replan-1791883157669.log
