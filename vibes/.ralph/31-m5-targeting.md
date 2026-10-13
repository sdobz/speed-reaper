# Ralph record: 31-m5-targeting

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- **Commit new files before `tools/check`.** The Windows cross build (`nix build .#reaper-plugin-windows` → `tools/windows-oracle --layer static`) builds from the flake source `src = ./.`, which only carries **git-tracked** files. Untracked sources referenced by CMakeLists make it fail with "Cannot find source file … No SOURCES given to target: speed_reaper" and the script dies silently (`2>/dev/null`).
- `cmake`/`ctest` are not on PATH outside the dev env: `nix develop --command …` (or run `tools/check`, which self-execs into it).


## Task: 1. `IProject` + `TargetItems` (pure)

### Attempt 1 — 2026-10-13T01:01:26Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- `cmake` is not on PATH outside the dev env; use `nix develop --command ...` (or just run `tools/check`, which self-execs into `nix develop`).
- Created `plugin/src/project.h` (IProject exactly per design §2), `plugin/src/target.h/.cpp` (TargetItems + WithTargetSelected template in header), `plugin/test/fake_project.h` (records SetSelectedItems calls in `selected_calls`, does NOT apply them), `plugin/test/target_test.cpp`; added `target.cpp` to `PLUGIN_PURE_SOURCES`.
- Rule 2 edge cases: boundary belongs to the item *starting* there (pos<=c<pos+len, items in pos order); cursor == pos+len of the track's LAST item → that item (1e-9 eps); else empty.
- IProject methods are non-const (per design §2) — helpers taking the project must take non-const refs.
- `target_test` alone passes (ctest -R target_test, 1/1).

Next:
- DONE. `tools/check target` exits 0 (ctest target_test 1/1, WINDOWS ORACLE PASS static, ORACLE PASS host). Line 16 ticked; work committed as `31-m5-targeting: 1. IProject + TargetItems (pure)`.

Dead ends:
- First background `tools/check target` (job `check-target`) failed rc=1 with no visible error: `tools/windows-oracle --layer static` dies silently because the flake cross build couldn't see untracked `target.cpp` (see Learnings). Fixed by committing; not a code bug.

Result: exit 0; done; 1 file changed, 1 insertion(+), 1 deletion(-); log: logs/31-m5-targeting-L16-a1-1791853286436.log

### Done — 2026-10-13 — 1 attempt(s)
Files:
```
M vibes/31-m5-targeting.md
?? vibes/.ralph/31-m5-targeting.md
```

## Task: 2. `ReaperProject` (REAPER-backed `IProject`)

### Attempt 2 — 2026-10-13T01:07:56Z — budget 1800s
Facts:
- Attempt 1 left no code; starting fresh.
- `plugin/src/project.h` (IProject) already exists from task 1; test fake at `plugin/test/fake_project.h`.
- `GetTrackAt`/`GetTrackIndex`/`GetSetLastTouchedTrack`/`GetSet_Loc` are NOT in the vendored `reaper_plugin_functions.h` (no `reaper.h` vendored). Subset doc §8 instead has `GetTrack(proj, idx)`, `CountTracks`, `GetLastTouchedTrack` — all also in `oracle/reaper_api_names.txt` (874 names, the host oracle's export list).
- `GetSet_Loc` is in NEITHER the subset doc NOR `reaper_api_names.txt` → resolve it via a manual `GetFunc("GetSet_Loc")` in a `ReaperProject::Init(void(*GetFunc)(const char*))` called from the entry after `REAPERAPI_LoadAPI`; oracle returns nullptr → `HasTimeSelection()` safely false. LOC_TSELSTART=0x20, LOC_TSELEND=0x21 (SDK reaper.h constants; could not verify online — web_search returned nothing, docs.reaper.com unreachable; step 5's oracle will exercise it).
- REAPERAPI pattern: main.cpp has `REAPERAPI_IMPLEMENT`+`REAPERAPI_MINIMAL`; reaper_project.cpp will define its own WANT set (mirrored in main.cpp) so a missing mirror is a link error on `REAPERAPI_<name>`.
- `tools/check` also runs `tools/reaper-oracle --layer host` (plugin_host + api names list) — all 12 new WANT names verified present in the list before adding them.
Next: write reaper_project.h/.cpp, add 12 WANT lines + Init call in main.cpp, add .cpp to plugin target in CMakeLists (NOT PLUGIN_PURE_SOURCES), commit, tools/check.
