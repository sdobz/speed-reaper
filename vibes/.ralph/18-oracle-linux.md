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
