# Ralph record: 32-m6-simple-verbs

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- REAPER-API rule (main.cpp comment): every new API a milestone adds needs `REAPERAPI_WANT_<name>` in BOTH plugin/src/reaper_project.cpp (or the calling TU) AND main.cpp's list (owns REAPERAPI_IMPLEMENT), or the plugin refuses to load.
- The host-layer oracle (plugin_host) resolves GetFunc for any name in oracle/reaper_api_names.txt (REAPER 7.81 export list, 874 names); all other names return nullptr.
- `MediaItem` is only forward-declared in the vendored SDK (reaper_plugin.h:1465) — you cannot dereference item fields; use the string APIs (GetMediaItemInfo_Value / SetMediaItemInfo_Value, e.g. "I_CURTAKE").
- tools/check needs the nix dev shell for cmake (`nix develop --command ...`); it runs: make -C src, plugin build with -Werror + SPEED_REAPER_FAKE_DEVICE=ON, nm export check, ctest, Windows cross build (tools/windows-oracle --layer static), oracle host layer. Set SKIP_WINDOWS=1 / ORACLE_FULL for variants.


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
