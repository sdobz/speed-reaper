# Ralph record: 05-prototypes

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- Run `nix build` and other long builds in the background (`setsid nohup ... > .ralph/logs/build.log 2>&1 &`) and poll; worker attempts are time-limited.

## Task: Cross-compile a windows version of speedcli

(Migrated from the pre-record notes file; raw notes are in logs/05-prototypes.notes.legacy.md.)

### Attempts 1–3 — 2026-10-11 — all timed out at 600s, no checkbox ticked
Facts:
- hidapi 0.14's Windows backend loads `hid.dll`/`cfgmgr32.dll` at runtime, so there's no link-time Win32 dependency, and a static self-contained build is possible.
- hidapi honours `BUILD_SHARED_LIBS=OFF`.
- nixpkgs `pkgs/by-name/hi/hidapi/package.nix` rejects a `cmakeFlags` argument (`.override` fails: "called with unexpected argument 'cmakeFlags'"). Use `overrideAttrs (prev: { cmakeFlags = (prev.cmakeFlags or []) ++ [ "-DBUILD_SHARED_LIBS=OFF" ]; })`.
- Partial work is uncommitted: `flake.nix` (+56 lines, a speedcli-windows output) and `src/Makefile`. Last eval error: .ralph/logs/05-prototypes-t4-a3-build.log.
Dead ends:
- `wineBin64Pre` doesn't exist in the locked nixpkgs.
- Re-reading nixpkgs source in every attempt used up the time budget.
Next: switch the hidapi override in flake.nix to `overrideAttrs`, then run `nix build .#speedcli-windows` in the background and check `file result/bin/speedcli.exe`.
Result: attempts exhausted (old ralph, before replanning existed)

### Attempt 1 — 2026-10-11T18:44:02Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 600s; no-progress; 3 files changed, 83 insertions(+), 9 deletions(-); log: logs/05-prototypes-L11-a1-1791744242204.log

### Replan (depth 0) — BLOCK → replan failed (no valid JSON decision block)
Trigger: 1 attempt(s), last: timeout after 600s, no-progress
Log: logs/05-prototypes-L11-replan-1791744874052.log
