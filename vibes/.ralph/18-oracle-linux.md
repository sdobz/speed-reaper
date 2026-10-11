# Ralph record: 18-oracle-linux

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).
- `tools/check [regex]` works from the worker shell: no `cmake` on PATH, but the script falls back to `nix develop` itself. Build is incremental in `build/plugin`; re-runs take ~10 s.
- `REAPERAPI_LoadAPI` (with `REAPERAPI_IMPLEMENT` + full `reaper_plugin_functions.h` include, as main.cpp does) probes **every** SDK API name — so the host's GetFunc list gates the whole entry result, not just what the plugin calls.


## Task: 1. Plugin trace

### Attempt 1 — 2026-10-11T22:11:46Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- Tree was clean before starting; wrote `plugin/src/trace.h/.cpp`, `plugin/test/trace_test.cpp`, added `trace.cpp` to `PLUGIN_PURE_SOURCES`, wired §2 messages + `SPEED_REAPER_TEST_CRASH=entry` abort into `ReaperPluginEntry` (all returns go through a `done(result)` lambda that traces `entry result=<n>`).
- `tools/check` has no `cmake` on the worker PATH; it auto-falls-back to `nix develop --command tools/check`, so run it via the script, not bare cmake.
- Trace design: env read per call (no caching), steady_clock since first call, mutex-guarded, `fprintf`+`fflush`; line = `<ms> <msg>\n`.
- `loadapi missing=<n>` uses the raw return value of `REAPERAPI_LoadAPI`; existing fail behavior (return 0) unchanged.

Dead ends:
- First run: `LineEndsWith` failed on the second trace line because it still had its trailing `\n` (only the first line was cut at `\n`). Fixed by popping the final `\n` before comparing.

Next:
- Done: `tools/check trace` green (log `vibes/.ralph/logs/check-trace2.log`), line 25 ticked.

Result: exit 0; done; 6 files changed, 77 insertions(+), 27 deletions(-); log: logs/18-oracle-linux-L25-a1-1791756706078.log

### Done — 2026-10-11 — 1 attempt(s)
Files:
```
M docs/plugin-design-m1-m3.md
 M vibes/.ralph/18-oracle-linux.md
 M vibes/20-m1-wheel-playhead.md
 M vibes/21-m2-buttons-modes.md
 M vibes/22-m3-edit-verbs.md
 M vibes/23-m4-shuttle.md
 M vibes/30-backlog.md
?? docs/plugin-oracle-design.md
?? vibes/19-oracle-device-windows.md
```

## Task: 2. The REAPER 7.81 API name list

### Attempt 2 — 2026-10-11T22:18:44Z — budget 600s

- Facts: `docs/reaper-api-subset.md` "Complete function index" section (line 793 to EOF) contains **874** backticked names (4 per line); the heading's "(924)" label is inaccurate — 874 is the true count. SDK header uses `(*REAPERAPI_FUNCNAME(Name))(`; 877 declarations total, 145 missing from the doc list (mostly REAPERAPI_EXPOSED functions, e.g. `FreeHeapPtr`, `get_config_var`).
- Facts: Deliverables done: `oracle/extract_api_names.py` (rerunnable), `oracle/reaper_api_names.txt` (874 lines, no dupes, `GetPlayPosition` present), `docs/notes/sdk-vs-reaper-api.md` (script stdout). Verify command passes.
- Next: none — task complete.

Result: exit 0; done; 2 files changed, 9 insertions(+), 1 deletion(-); log: logs/18-oracle-linux-L38-a2-1791757124621.log

### Done — 2026-10-11 — 2 attempt(s)
Files:
```
M vibes/.ralph/18-oracle-linux.md
 M vibes/18-oracle-linux.md
?? docs/notes/
?? oracle/
```

## Task: 3. `plugin_host` (Linux)

### Attempt 3 — 2026-10-11T22:20:14Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Facts:
- SDK `reaper_plugin_info_t` layout (vendor/reaper-sdk/sdk/reaper_plugin.h): `{int caller_version; HWND; int (*Register)(const char*, void*); void* (*GetFunc)(const char*)}`; caller_version = 0x20E; on Linux HWND is void*.
- Wrote `plugin/tools/plugin_host.cpp` (dlopen/LoadLibrary→Linux dlopen only; own struct copy of `reaper_plugin_info_t` with variadic Register since the plugin casts to `plugin_register_variadic`; one `extern "C"` stub for all listed names; `custom_action` → ids 40001+; `timer` → (id, ms, fn) captured; `--expect-actions` matches id_str or cmd number).
- CMake: `add_executable(plugin_host tools/plugin_host.cpp)` + `target_link_libraries(... dl)`, guarded by `if(EXISTS ...)` like test/. Output lands at `build/plugin/plugin_host`.
- Build + Verify pass (exit 0), log `vibes/.ralph/logs/build.log`. Host output: `api_names=... (874) / loaded / entry=0 registered= / actions= / timers= / ticks=50 / unloaded`.
- Why entry=0 today (expected): main.cpp does `#define REAPERAPI_IMPLEMENT` + `#include "reaper_plugin_functions.h"`, so `REAPERAPI_LoadAPI` probes **every** SDK API name; the oracle list is the ReaScript doc index and is missing ~145 SDK names → missing>0 → plugin returns 0 before any Register. Generic stub is indeed never called.
- Next: none — line 51 ticked.

Result: exit 0; done; 3 files changed, 25 insertions(+), 1 deletion(-); log: logs/18-oracle-linux-L51-a3-1791757214456.log

### Done — 2026-10-11 — 3 attempt(s)
Files:
```
M plugin/CMakeLists.txt
 M vibes/.ralph/18-oracle-linux.md
 M vibes/18-oracle-linux.md
?? plugin/tools/
```

## Task: 4. Oracle tools in the dev shell

### Attempt 4 — 2026-10-11T22:26:53Z — budget 600s
- Facts: `gdb` was already in devShell buildInputs; added `xvfb-run`, `xorg-server`, `strace`, `xdotool`, `python3` (one commented block after `reaper`) plus one banner line mentioning `tools/reaper-oracle`. Verify command exits 0 (xvfb-run 1+g87f6705, Xvfb xorg-server-21.1.24, gdb 17.2, strace 7.2, reaper 7.81).
- Next: task 4 done; proceed to task 5 (`tools/reaper-oracle`).

Result: exit 0; done; log: logs/18-oracle-linux-L64-a4-1791757613442.log

### Done — 2026-10-11 — 4 attempt(s)
Files:
```
M vibes/.ralph/18-oracle-linux.md
```

## Task: 5. `tools/reaper-oracle`: host layer + real-REAPER `load` scenario

### Attempt 5 — 2026-10-11T22:27:41Z — budget 600s

- Facts:
  - `tools/reaper-oracle` done: option parsing, re-exec into `nix develop` (gate on `command -v reaper`), host layer (tools/check cmake flags + `plugin_host --expect-entry 1 --expect-actions …`, stdout appended to `trace.txt`), reaper layer (isolated wiped `HOME=build/oracle/home`, plugin copied to `UserPlugins/`, `timeout 60 xvfb-run -a reaper -newinst -nosplash oracle/scenarios/<name>.lua`, `SPEED_REAPER_TRACE`/`ORACLE_LIB`/`ORACLE_OUT` set, optional `oracle/scenarios/<name>.env` sourced), `result.json` derived in python3 from `probe.json` + exit codes. No gdb (task 6).
  - `oracle/lib.lua`: `check`, `record`, `wait_ms(ms, fn, on_done)`, `finish()` (writes `probe.json` = `{checks, info}`, quits via `Main_OnCommand(40004,0)`); hand-rolled JSON encoder. Names also exposed unqualified; scenarios still do `dofile(os.getenv("ORACLE_LIB"))`.
  - `oracle/scenarios/load.lua`: 3 action checks via `reaper.NamedCommandLookup("_"..id)`, records `app_version`.
  - Real REAPER 7.81 runs under xvfb fine (~10 s with `--report-only`); `reaper -nosplash file.lua` works. In real REAPER `loadapi missing=1` (host: 145) — useful for task 7.
  - Both layers currently FAIL as expected (`entry=0`, all `NamedCommandLookup`=0). `--report-only` → exit 0; without it → exit 1. Verify passes.
- Dead ends:
  - First version lost CLI args on re-exec: the option-parsing loop `shift`s away `$@` before `exec nix develop --command "$0" "$@"`. Fix: save `ORIG_ARGS=("$@")` before parsing and re-exec with it.
- Next:
  - Task 6: crash capture (gdb `bt.txt`, `LD_DEBUG=libs` → `reaper.stderr`, `noop.lua`, exit ≥128/124 → reason mentions crash/timeout). Reuse `REAPER_RC` handling already in `run_reaper`; note timeout=124, and `run_reaper` currently does not distinguish them.

Result: exit 0; done; log: logs/18-oracle-linux-L78-a5-1791757661139.log

### Done — 2026-10-11 — 5 attempt(s)
Files:
```
M vibes/.ralph/18-oracle-linux.md
```

## Task: 6. Crash capture

### Attempt 6 — 2026-10-11T22:34:28Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts:
  - `SPEED_REAPER_TEST_CRASH` hook already existed in `plugin/src/main.cpp` (aborts right after `Trace("entry rec=non-null")`, before `REAPERAPI_LoadAPI`) and was already in the built `.so`; no plugin change needed.
  - `reaper` on PATH is a nix wrapper **script** (`bin/reaper` -> `../opt/REAPER/reaper`, a bash launcher that sets LD_LIBRARY_PATH and `exec`s `opt/REAPER/.reaper-wrapped`). gdb cannot exec scripts ("not in executable format"). Fix: `gdb -batch -ex "set exec-wrapper <launcher>" -ex run -ex bt --args <.../opt/REAPER/.reaper-wrapped> ...` — the wrapper exec's, so the ELF is a direct child of gdb.
  - `result.json` reason already said `crash`/`timeout` for exit >=128/124 from task 5; only the capture artifacts were missing.
  - Added `oracle/scenarios/noop.lua` (`finish()`), `run_crash_capture()` in `tools/reaper-oracle` (gdb -> `build/oracle/bt.txt`; `LD_DEBUG=libs` run filtered by `grep -i speed_reaper` -> `reaper.stderr`), triggered when `REAPER_RC >= 128 || == 124`.
  - bt.txt on crash: `SIGABRT` ... `#3 ReaperPluginEntry.cold () from .../UserPlugins/reaper_speed_reaper.so`. Verify passes; task-5 `--report-only` load run still writes all artifacts.
- Dead ends:
  - `gdb -batch ... --args reaper` directly: gdb rejects the nix wrapper script; `bt` is empty ("No stack").
- Notes for task 7: `reaper.stderr` (LD_DEBUG) shows `calling init: .../UserPlugins/reaper_speed_reaper.so` then `error: symbol lookup error: undefined symbol: SWELL_dllMain (fatal)` — but the plugin DOES load (dlopen RTLD_NOW in isolation is clean; trace shows `entry rec=non-null`), so this is REAPER-side (reaper ELF exports `SWELLAPI_GetFunc`/`SWELLAppMain` but no `SWELL_dllMain`). Load failure is `loadapi missing` — see trace.txt.

Result: exit 0; done; log: logs/18-oracle-linux-L91-a6-1791758068437.log

### Done — 2026-10-11 — 6 attempt(s)
Files:
```
M vibes/.ralph/18-oracle-linux.md
```

## Task: 7. Diagnose the load failure with the oracle

### Attempt 7 — 2026-10-11T22:40:21Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->

Result: timeout after 600s; progress; 1 file changed, 5 insertions(+); log: logs/18-oracle-linux-L104-a7-1791758421687.log

### Replan (depth 0) — SPLIT op=AND → "Probe REAPER 7.81 for the one missing SDK name", "Write docs/notes/oracle-load-diagnosis.md"
Trigger: 7 attempt(s), last: timeout after 600s, progress
Why: Attempt 7 gathered all evidence and built a working GetFunc probe (oracle/probe_missing.c, build/oracle/probe_missing.so, build/oracle/sdk_api_names.txt), then spent the rest of the budget on compile errors and a hung manual REAPER run; only one probe run and the note itself remain.
Rejected: BUDGET — the hard-won facts (working g++ command, probe already built) live only in the log and working tree, so extra time just risks re-hitting the same walls. REWRITE — the task text and Verify are fine; de-scoping the exact missing name would weaken the note when the probe already exists. SPIKE — the open question is half-answered in the tree, a fresh research step would duplicate it. BLOCK — no human decision is needed.
Log: logs/18-oracle-linux-L104-replan-1791759066015.log

## Task: Probe REAPER 7.81 for the one missing SDK name

### Attempt 1 — 2026-10-11T22:59:57Z — budget 600s
- Facts: probe .so already built at build/oracle/probe_missing.so (15712 bytes); sdk names file was at build/oracle/sdk_api_names.txt (877 lines) — copied to oracle/sdk_api_names.txt as the task's run command expects.
- Facts: probe run launched in background, log at vibes/.ralph/logs/probe-run.log.
- Dead ends: first two probe runs (timeout 45, with and without LD_DEBUG=libs) produced no probe_missing.txt; /tmp/lddbg.out shows REAPER never dlopened anything from UserPlugins (bundled Plugins all fail dlopen with `undefined symbol: DllMain (fatal)` — harmless, same in oracle runs).
- Facts: root bug found via `nm -D`: probe .so exported only the C++-mangled `_Z17ReaperPluginEntryPKcS0_iPKv`; REAPER dlsym()s the unmangled `ReaperPluginEntry` (real plugin exports it via `extern "C"` in plugin/src/main.cpp:86). Applied the one-word `extern "C"` fix to oracle/probe_missing.c (not a rewrite) and rebuilt with the GTK-includes g++ command from the a7 log.
- Facts: run 3 in background (timeout 90), log vibes/.ralph/logs/probe-run3.log.

Result: timeout after 600s; progress; 2 files changed, 12 insertions(+), 1 deletion(-); log: logs/18-oracle-linux-L106-a1-1791759597934.log

### Attempt 2 — 2026-10-11T23:10:38Z — budget 600s
- Facts: probe runs 1-4 (timeout 45/90, with and without LD_DEBUG=libs) never reached the UserPlugins dlopen — at 90 s REAPER was still processing bundled Plugins (harmless `DllMain`/`SWELL_dllMain` failures); logs vibes/.ralph/logs/probe-run*.log, /tmp/lddbg2.out.
- Facts: offline cross-check: scanned the REAPER 7.81 ELF (/nix/store/h2crs2mvvh5ig710x47qnwcazynhc3fc-reaper-7.81/opt/REAPER/.reaper-wrapped, 12.8 MB, plus opt/REAPER *.so) for all 877 SDK names from oracle/sdk_api_names.txt — exactly ONE name absent: `EnumThemeColors` (present in 876/877). Matches reaper-layer trace `0 loadapi missing=1`.
- Facts: EnumThemeColors IS in oracle/reaper_api_names.txt (host list, 874), so it is NOT among the host's 145 missing (sdk-vs-reaper-api.md list); host 145 = SDK 877 minus 732 overlap.
- Facts: draft docs/notes/oracle-missing-names.md written (PROBE line + EnumThemeColors + method + 145 note).
- Next: background probe run 5 (timeout 240, no LD_DEBUG, log vibes/.ralph/logs/probe-run5.log, out /tmp/probe_long.log + build/oracle/probe_missing.txt); if it lands a PROBE line, keep note as-is; else amend note to say the .so probe could not be made to run and the name comes from the ELF scan (still not invented).

Result: timeout after 600s; progress; 2 files changed, 21 insertions(+), 1 deletion(-); log: logs/18-oracle-linux-L106-a2-1791760238488.log

### Attempt 3 — 2026-10-11T23:21:20Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: no reaper/xvfb process alive; probe_missing.txt still absent after run 5 (timeout 240). Run 5's reaper did start (jack errors in /tmp/probe_long.log) but never wrote the PROBE output — standalone probe runs never get past REAPER's bundled-Plugins scan, while tools/reaper-oracle's reaper runs (fresh HOME, timeout 60, with a .lua scenario arg) DO reach UserPlugins entry within 60 s (build/oracle/trace.txt shows 3× `0 loadapi missing=1`).
- Facts: probe .so is the fixed rebuild (extern "C", mtime 16:06) and is already copied into build/oracle/home/.../UserPlugins/ by run 5.
- Facts: offline ELF scan (attempt 2) remains the authoritative measurement: exactly 1 of 877 SDK names absent from REAPER 7.81 = EnumThemeColors; matches reaper-layer trace missing=1.
- Facts: run 6 (timeout 300) finished with no build/oracle/probe_missing.txt — the .so probe could not be made to run (REAPER never reaches the UserPlugins dlopen in standalone runs); 6 total runs logged.
- Facts: re-ran the offline ELF scan in this session — reproduced exactly: 1 of 877 names absent from REAPER 7.81 = EnumThemeColors (whole-token grep yields 5 extra names that are substrings of longer symbols, e.g. GetTake in GetTakeID; substring scan is the correct one).
- Next: none — docs/notes/oracle-missing-names.md written (PROBE line, name, honest method, .so-probe caveat, 145 note); line 106 ticked.

Result: exit 0; done; 3 files changed, 33 insertions(+), 2 deletions(-); log: logs/18-oracle-linux-L106-a3-1791760880446.log

### Done — 2026-10-11 — 3 attempt(s)
Files:
```
M oracle/probe_missing.c
 M vibes/.ralph/18-oracle-linux.md
 M vibes/18-oracle-linux.md
?? docs/notes/oracle-missing-names.md
?? oracle/sdk_api_names.txt
```

## Task: Write docs/notes/oracle-load-diagnosis.md

### Attempt 1 — 2026-10-11T23:30:36Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
- Facts: `build/oracle/` artifacts were fresh and complete (trace.txt, result.json, reaper.stderr, bt.txt); no re-run of `tools/reaper-oracle --report-only` needed.
- Facts: host trace `0 entry rec=non-null` / `0 loadapi missing=145` / `0 entry result=0`, zero `register` lines; reaper layer `0 loadapi missing=1` / `0 entry result=0`.
- Facts: `docs/notes/oracle-missing-names.md` already names the single reaper-layer missing name: `EnumThemeColors`.
- Facts: wrote `docs/notes/oracle-load-diagnosis.md` with quoted evidence, `Root cause:` and `Fix:` lines; verify command passes.
- Next: none — line 114 ticked; task 8 applies the `REAPERAPI_MINIMAL` fix in `plugin/src/main.cpp`.

Result: exit 0; done; 2 files changed, 11 insertions(+), 1 deletion(-); log: logs/18-oracle-linux-L114-a1-1791761436238.log

### Done — 2026-10-11 — 1 attempt(s)
Files:
```
M vibes/.ralph/18-oracle-linux.md
 M vibes/18-oracle-linux.md
?? docs/notes/oracle-load-diagnosis.md
```

## Task: 8. Fix the load failure

### Attempt 1 — 2026-10-11T23:31:51Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Only two REAPER API functions are called by plugin code: `CSurf_ScrubAmt` + `GetPlayPosition` (both in `ReaperPlayheadSink`, main.cpp; jog_engine/trace are REAPER-free). Both names are in `oracle/reaper_api_names.txt` (host layer, 874 names).
- Applied the note's `Fix:`: `#define REAPERAPI_MINIMAL` + two `REAPERAPI_WANT_` lines (one per called function) before including `reaper_plugin_functions.h`, with the rule ("every function a milestone adds needs its REAPERAPI_WANT line in the same change") as a comment above the list. `REAPERAPI_IMPLEMENT` kept.
- Exit-139-on-quit: `bt.txt` SIGSEGV backtrace is 6 frames entirely inside REAPER's `.reaper-wrapped` (no plugin frames; `0 unload` trace line completed normally) → not our code; recorded in `docs/notes/oracle-load-diagnosis.md` per the task and moving on.

Result: timeout after 600s; progress; 4 files changed, 23 insertions(+), 1 deletion(-); log: logs/18-oracle-linux-L133-a1-1791761511767.log

### Attempt 2 — 2026-10-11T23:42:34Z — budget 600s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Attempt 1's tree already contains the complete fix (REAPERAPI_MINIMAL + 2 REAPERAPI_WANT lines + rule comment in plugin/src/main.cpp; exit-139 note in docs/notes/oracle-load-diagnosis.md). No rework needed.
- Attempt 1's verify actually passed: `vibes/.ralph/logs/task8-verify.log` ends with `PASS=True` / `ORACLE PASS all/load`; `build/oracle/result.json` pass=true (entry=1, 3 actions registered on REAPER 7.81). Timeout was the worker's poll loop, not the verify.
- `reaper --version` does not hang (exits 0 in <10 s, even with stdin=/dev/null, no display) — but it dumps 63 lines of usage into the nix develop greeting. Attempt 1's flake.nix change (greeting shows `reaper   ...` instead) is kept: cosmetic + avoids the usage dump; verified non-load-bearing but harmless.
- Re-running full `tools/check && tools/reaper-oracle --layer all --scenario load` in background: log `vibes/.ralph/logs/task8-verify-a2.log` (grep for `VERIFY_EXIT=`).
Next: confirm VERIFY_EXIT=0 in task8-verify-a2.log, tick line 133, stop.

Result: exit 0; done; 5 files changed, 35 insertions(+), 2 deletions(-); log: logs/18-oracle-linux-L133-a2-1791762154259.log

### Done — 2026-10-11 — 2 attempt(s)
Files:
```
M docs/notes/oracle-load-diagnosis.md
 M flake.nix
 M plugin/src/main.cpp
 M vibes/.ralph/18-oracle-linux.md
 M vibes/18-oracle-linux.md
```
