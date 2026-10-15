# Ralph record: 40-cleanup

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/reaper-oracle --layer all`: the `verbs` scenario is STALE (pre-existing since M8, as of 40-cleanup task 2): it expects exactly the 8 bare verb cmd ids (impl wraps RIPL DEL in ripple set/restore 40310/40309 when ripple is off) and `led main=` of only 0x20/0x00 (impl also sets LED_CAM1 0x4000 for selected single-take items). Expect `ORACLE FAIL all/...: verbs` until someone rewrites the scenario or the impl.
- Clean-HEAD baselines via `git worktree` are not usable for the REAPER layer: REAPER needs an absolute `$HOME`, but `tools/reaper-oracle` passes the relative `build/oracle/home` → "Failed to get the home directory" → scenario timeout. Baseline in the main tree instead (e.g. `git stash` + run + `git stash pop`).
- Nix builds (e.g. `nix build .#reaper-plugin-windows`, `src = ./.`) copy the repo via a path dep that IGNORES untracked files in a dirty git tree (the /nix/store/...-source copy lacks them → "fatal error: <newfile>: No such file or directory" while edited tracked files ARE present). `git add` new files before any nix build; the build then re-evaluates to a new source path.


## Task: 1. Repo hygiene

### Attempt 1 — 2026-10-15T02:21:56Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - `oracle/scenarios/spike_xfade.lua` was ALREADY tracked (committed in 235b32a) — nothing to do there.
  - `.gitignore` already covered `oracle/__pycache__/` (via `__pycache__/`) and `build/oracle/` (via `build/`); added `vibes/.ralph-logs/` + `build/oracle/` explicitly anyway; `git rm -r --cached vibes/.ralph-logs` done.
  - The six probes are not in `REAPER_SCENARIOS` in `tools/reaper-oracle` (only `probe.json` output names match `probe_`). Deleted.
  - Docs citing probes, citation replaced by finding: `docs/notes/m4-shuttle-api.md` (scenario path → "since-deleted one-off scenario"), `docs/plugin-oracle-design.md` (scenario catalogue line → pointer to m4-shuttle-api.md), `docs/notes/m5-observations.md` (stripped probe names, findings stay), `vibes/32-m6-simple-verbs.md` (line 90), `vibes/23-m4-shuttle.md` (line 42). Also stripped two comment citations in `oracle/scenarios/smth_cut.lua` (in-suite scenario, would have been dangling). vibes/23 line 43/48 keep `--scenario spike_rate` as historical command invocations of a checked-off task, not path citations.
- Dead ends:
  - none so far
- Next:
  - DONE: `tools/check` green (job check40-1), full Verify rc=0, line 24 ticked, committed as `40-cleanup: 1. Repo hygiene`.

Result: exit 0; done; log: logs/40-cleanup-L24-a1-1792030916691.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
?? vibes/.ralph/40-cleanup.md
```

## Task: 2. Delete the hello-world demo and the M4 spike code

### Attempt 2 — 2026-10-15T02:26:43Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - Test file is `plugin/test/jog_engine_test.cpp` (task said `plugin/src/…` — wrong path in task).
  - `Master_GetPlayRate`, `CSurf_OnPlayRateChange`, `OnStopButton` are still called by shuttle (`ApplyShuttleRate`, `HardStop` in main.cpp) → keep the 3 `REAPERAPI_WANT_*` lines, only strip "spike" from their comments.
  - `jog_engine.cpp` contains only `DemoWheelSource` methods → delete file + its line in `plugin/CMakeLists.txt` (line 52).
  - Oracle fallout beyond the task text: `oracle/scenarios/load.lua` checks the 3 demo action ids via `NamedCommandLookup` and `run_host()` in `tools/reaper-oracle` passes `--expect-actions speed_reaper_jogdemo,…` to `plugin_host` — both must be updated or the reaper/host layers fail. Plugin registers zero actions after the delete.
  - `load.lua` rewrite: plugin no longer registers actions, so it now checks the trace file (lib.lua `read_trace()`/`find_line()` work in scenarios; `SPEED_REAPER_TRACE` is set per scenario) for `entry result=1` / `register timer ok` / `hid source started`.
  - `SummedWheelSource`/`NullWheelSource`/`JogEngine::NudgeSeconds` had no callers outside main.cpp + jog_engine_test.cpp → all four demo types deleted from `jog_engine.h`; engine constructed directly from `g_gated_source`.
  - `onHookCommand2` + `RegisterAction` helper + `hookcommand2` registration removed entirely (no actions remain to hook).
  - SMOKE-TEST.md: Build/Install/Uninstall + gdk.h note + Windows cross-build moved into numbered Prerequisites 5–7; M1 step 3's "hello-world" pointer updated to "Prerequisites 5–6".
  - Progress: all edits done. `tools/check` green (job check40-2: 14/14 unit tests, host oracle PASS, Windows build PASS). `tools/reaper-oracle --layer reaper` (runs only `load` — the full suite needs `--layer all`) PASSED with the rewritten load.lua. Full suite (`--layer all`, job suite40-2) running as regression check — note it in Next.
  - One dangling pointer fixed along the way: flake.nix dev-shell banner line 177 pointed at the deleted SMOKE-TEST 'REAPER plugin' section → now "Prerequisites 5–6". (docs/reaper-dev-env.md "hello world" hits are about the upstream reaper-sdk-vscode template — left alone.)
- Facts:
  - suite40-2 (`--layer all`): ALL scenarios pass except `verbs` — 2 failing checks: (a) `cmd lines are the 8 verb ids` — trace shows `40310 40006 40309` (verbs.cpp RIPL DEL wraps delete in ripple set/restore when ripple is OFF, which is the fresh-project default) vs the scenario's expected bare 8 ids; (b) `led main=` final value 0x4000 (LED_CAM1 = shared active take 1 on the selected single-take items) vs expected 0x20/0x00. Both live in verbs.cpp/led_state/reaper_project.cpp — untouched by this task. Suspect pre-existing (no `--layer all` run since M8).
  - Baseline check: clean-HEAD worktree at /tmp/sr-clean (job baseline-verbs) runs `--scenario verbs` to prove the failure predates my diff.
  - Baseline worktree at /tmp/sr-clean was environmentally broken (REAPER "Failed to get the home directory, not an absolute path: build/oracle/home" → scenario timeout; worktree removed again). NOT a valid baseline — do not retry the worktree approach; REAPER wants an absolute HOME.
  - Pre-existing verdict instead via code inspection: the 2 failing `verbs` checks depend only on `verbs.cpp` RunRippleDelete (ripple wrap 40310/40006/40309 when ripple off — fresh-project default) and `ReadLedState`/`MainLedMask` (selected single-take items → active_take=1 → LED_CAM1 bit 0x4000; scenario expects only 0x20/0x00). `git diff --stat` on verbs.cpp/reaper_project.cpp/led_state.*/gesture.*/verbs.lua = empty: my diff touches none of it. ⇒ `--layer all` verbs failure predates this task (no full-suite run since M8 landed the LED bits + ripple wrap). The task Verify command runs `--layer reaper` = `load` scenario only, which passes.
- Next:
  - DONE: verify40-2 (exact Verify command) rc=0 — tools/check green (14/14 unit tests, host + Windows oracle PASS), grep clean, `ORACLE PASS reaper/load`. Line 39 of vibes/40-cleanup.md ticked. Job names: check40-2 (tools/check), oracle40-2 (--layer reaper), suite40-2 (--layer all, verbs pre-existing failure), verify40-2 (exact Verify).

Result: exit 0; done; log: logs/40-cleanup-L39-a2-1792031203704.log

### Done — 2026-10-15 — 2 attempt(s)
Files:
```
M vibes/.ralph/40-cleanup.md
```

## Task: 3. One REAPER API list

### Attempt 3 — 2026-10-15T02:42:24Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

- Facts:
  - Clean tree (only record file modified); no partial work from earlier attempts.
  - Union of the two old lists = 39 unique names (main.cpp had 41 lines incl. 2 dupes: GetCursorPosition, GetToggleCommandState).
  - `reaper_plugin_functions.h` lives at `vendor/reaper-sdk/sdk/` (SYSTEM include dir); guarded by `_REAPER_PLUGIN_FUNCTIONS_H_`, defines globals only when `REAPERAPI_IMPLEMENT`, else externs + `REAPERAPI_LoadAPI` declaration. Safe to include from a shared header.
  - New `plugin/src/reaper_api.h` = MINIMAL + 39 WANT lines (comment = caller) + `#include "reaper_plugin_functions.h"`. main.cpp: `#define REAPERAPI_IMPLEMENT` then include; reaper_project.cpp: plain include. All duplicated lists + "keep in sync" comments deleted.
  - New `tools/check_api_want` (python3): strips comments/string literals (state machine incl. raw strings) from `plugin/src/*.cpp`, whole-word-matches names from `oracle/sdk_api_names.txt`, fails listing missing WANT names. Comment-only mentions (CSurf_OnScroll, CSurf_ScrubAmt, plugin_register) do NOT count — that's why stripping is required. Negative-tested (removing a line → FAIL rc=1).
  - Wired `tools/check_api_want` into `tools/check` right after the nix-develop guard, before the build.
  - check40-3 FAILED at the Windows cross build: nix source copy lacked the untracked `reaper_api.h` (see Learnings). Fixed by `git add plugin/src/reaper_api.h tools/check_api_want`; rebuild win40-3 rc=0.
  - Also fixed before verify: the comment in reaper_project.cpp originally contained the literal `REAPERAPI_WANT_*` — the Verify `rg -c` counts comment occurrences too, so it would have made 2 files match; reworded the comment.
  - DONE: verify40-3 (exact Verify command) rc=0 — tools/check green (check_api_want OK, 14/14 unit tests, WINDOWS ORACLE PASS static, ORACLE PASS host), `rg -c` count = 1 (only reaper_api.h), ORACLE PASS reaper/load. Line 51 ticked.
