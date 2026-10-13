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
