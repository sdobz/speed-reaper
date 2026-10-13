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
