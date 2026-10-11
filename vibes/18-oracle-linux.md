# Oracle (Linux): an agent-runnable verdict on "does the plugin load and work"

The plugin currently fails to load in real REAPER on Linux (probe: its actions
are not registered; REAPER exits 139 on quit). No human may be in the debug
loop, so build the oracle first and use it to diagnose and fix the failure.
Design (commands, artifacts, trace format, host, scenarios) is settled in
`docs/plugin-oracle-design.md` — read the § each task names.
Hypothesis to confirm, not assume: `REAPERAPI_LoadAPI` (non-`REAPERAPI_MINIMAL`
build) returns nonzero when REAPER lacks any of ~900 SDK functions, and
`main.cpp` then returns 0 from `ReaperPluginEntry`.
Windows and the fake device come next in `19-oracle-device-windows.md`.

### 1. Plugin trace

- Create `plugin/src/trace.h/.cpp` per `docs/plugin-oracle-design.md` §2 (`Trace(fmt, ...)`, env `SPEED_REAPER_TRACE`); add `trace.cpp` to `PLUGIN_PURE_SOURCES`.
- Add `plugin/test/trace_test.cpp`: with the env var set to a temp file, two `Trace` calls produce two lines each ending in the message; with it unset, no file is created.
- In `plugin/src/main.cpp` `ReaperPluginEntry`: emit the §2 required messages (`entry rec=…`, `loadapi missing=<n>`, each `register … ok|FAIL`, `action <id> cmd=<n>`, `entry result=…`, `unload`). Add the `SPEED_REAPER_TEST_CRASH=entry` hook.
- `loadapi missing=<n>` is just the return value of `REAPERAPI_LoadAPI`; listing names comes later (task 8). Do not change any other behavior or fix the load failure yet.

**Verify**
```bash
tools/check trace
```

- [x] Done

### 2. The REAPER 7.81 API name list

- Create `oracle/reaper_api_names.txt`: one exported function name per line, extracted from the "Complete function index" section of `docs/reaper-api-subset.md` (the backticked names). Keep the extractor as `oracle/extract_api_names.py` so it can be rerun.
- Also print, to stdout of the script, the count and the SDK functions declared in `vendor/reaper-sdk/sdk/reaper_plugin_functions.h` (pointer declarations `(*Name)(`) that are **missing** from the list; save that output to `docs/notes/sdk-vs-reaper-api.md`.
- Do not edit the plugin or SDK.

**Verify**
```bash
test "$(wc -l < oracle/reaper_api_names.txt)" -gt 800 && grep -qx GetPlayPosition oracle/reaper_api_names.txt && test -s docs/notes/sdk-vs-reaper-api.md
```

- [x] Done

### 3. `plugin_host` (Linux)

- Implement `plugin/tools/plugin_host.cpp` and target `plugin_host` (output `build/plugin/plugin_host`, links `dl`) exactly per design §3. Read `oracle/reaper_api_names.txt` from the path given by env `ORACLE_API_NAMES` (default `oracle/reaper_api_names.txt` relative to the cwd).
- `GetFunc` must return nullptr for names not in the list (that is the point). Stubs may be one generic function; never call them.
- Do not change the plugin. It is expected that the host reports `entry=0` today.

**Verify**
```bash
tools/check && ./build/plugin/plugin_host build/plugin/reaper_speed_reaper.so --report-only | grep -q '^entry='
```

- [x] Done

### 4. Oracle tools in the dev shell

- In `flake.nix` devShell packages add `xvfb-run`, `xorg-server` (for `Xvfb`), `gdb`, `strace`, `xdotool`, `python3`. Keep everything else.
- Update the shell banner minimally (one line mentioning `tools/reaper-oracle`).
- Do not add wine (task in `19-…`).

**Verify**
```bash
nix develop --command bash -c 'command -v xvfb-run && command -v Xvfb && command -v gdb && command -v strace && command -v reaper'
```

- [x] Done

### 5. `tools/reaper-oracle`: host layer + real-REAPER `load` scenario

- Write `tools/reaper-oracle` (bash, `set -euo pipefail`, re-exec in `nix develop` like `tools/check`) per design §1 and §4: options, `build/oracle/` artifacts, final `ORACLE PASS|FAIL` line, `result.json`, isolated `HOME`, `--report-only`.
- `host` layer: build with `tools/check`'s cmake flags plus run `plugin_host … --expect-entry 1 --expect-actions speed_reaper_jogdemo,speed_reaper_scrub_fwd,speed_reaper_scrub_back`; store its output in `trace.txt` alongside plugin trace.
- `reaper` layer: create `oracle/lib.lua` (design §4 API) and `oracle/scenarios/load.lua` (one `check` per action id via `NamedCommandLookup("_<id>")` ≠ 0; also records `reaper.GetAppVersion()`); implement the §4 run (xvfb-run, timeout 60, trace env).
- Crash capture is the next task; do not implement gdb here. Do not fix the plugin.

**Verify**
```bash
tools/reaper-oracle --report-only >/dev/null; test -s build/oracle/result.json && grep -q '"layer"' build/oracle/result.json && test -s build/oracle/trace.txt
```

- [x] Done

### 6. Crash capture

- In `tools/reaper-oracle`, on REAPER exit status ≥128 or timeout: re-run the same command under `gdb -batch -ex run -ex bt --args …` writing `build/oracle/bt.txt`, and a `LD_DEBUG=libs` run filtered to lines mentioning `speed_reaper` into `reaper.stderr`. The `result.json` `reason` must mention `crash` or `timeout`.
- Add scenario `oracle/scenarios/noop.lua` (just `finish()`), usable with `--scenario noop`.
- Prove it with the test hook: `SPEED_REAPER_TEST_CRASH=entry tools/reaper-oracle --layer reaper --scenario noop --report-only` must produce a `bt.txt` containing `ReaperPluginEntry`.

**Verify**
```bash
SPEED_REAPER_TEST_CRASH=entry tools/reaper-oracle --layer reaper --scenario noop --report-only >/dev/null; grep -q ReaperPluginEntry build/oracle/bt.txt && grep -qi crash build/oracle/result.json
```

- [x] Done

### 7. Diagnose the load failure with the oracle

- Run `tools/reaper-oracle --report-only` and read `build/oracle/*`. Write `docs/notes/oracle-load-diagnosis.md`: the evidence (quote the `entry`/`loadapi`/`register` trace lines, `result.json` reason, `bt.txt` or `reaper.stderr` lines), a line starting `Root cause:`, and a line starting `Fix:` with the exact change (file + what).
- If the cause is `loadapi missing=<n>` with n>0, name the missing functions using `docs/notes/sdk-vs-reaper-api.md` from task 2.
- Do not change plugin code in this task.

**Verify**
```bash
grep -q '^Root cause:' docs/notes/oracle-load-diagnosis.md && grep -q '^Fix:' docs/notes/oracle-load-diagnosis.md
```

- [x] Done
  <!-- ralph: replanned=1 op=AND -->
  - [x] Probe REAPER 7.81 for the one missing SDK name
    Attempt 7 left a built probe: `oracle/probe_missing.c` (source), `build/oracle/probe_missing.so` (built), `build/oracle/sdk_api_names.txt` (877 SDK names). Use these; do not rewrite the probe.
    If the .so is missing, rebuild it with the exact g++ command in `vibes/.ralph/logs/18-oracle-linux-L104-a7-1791758421687.log` (needs `-Ivendor/reaper-sdk/sdk -Ivendor/WDL/WDL` plus the gtk+3.24.52 nix-store includes; no pkg-config on PATH).
    Run (fresh HOME, probe only, NO .lua script — it writes its output at plugin entry; the timeout kill is expected): `nix develop --command bash -lc 'rm -rf build/oracle/home && mkdir -p build/oracle/home/.config/REAPER/UserPlugins && cp build/oracle/probe_missing.so build/oracle/home/.config/REAPER/UserPlugins/ && HOME=$PWD/build/oracle/home ORACLE_SDK_NAMES=$PWD/oracle/sdk_api_names.txt ORACLE_PROBE_OUT=$PWD/build/oracle/probe_missing.txt timeout 45 xvfb-run -a reaper -newinst -nosplash; true'`
    Expected: `build/oracle/probe_missing.txt` ends in `PROBE total=877 missing=N` with N name lines above it (N likely 1). If no PROBE line appears, re-run once with `LD_DEBUG=libs` appended to check the .so is loaded, then stop diagnosing.
    Write `docs/notes/oracle-missing-names.md`: the PROBE line + missing name(s), method in one line, and note that the host layer's 145 missing names are exactly the list in `docs/notes/sdk-vs-reaper-api.md`. If the probe cannot be made to run, say so explicitly and list the 145 as the candidate set — do not invent a name.
    Do not touch plugin code, `tools/reaper-oracle`, or the real plugin .so.
    Verify: `test -s docs/notes/oracle-missing-names.md && grep -q '^PROBE total=' docs/notes/oracle-missing-names.md`
  - [x] Write docs/notes/oracle-load-diagnosis.md
    Write `docs/notes/oracle-load-diagnosis.md` from the fresh `build/oracle/` artifacts (load scenario; re-run `tools/reaper-oracle --report-only` only if they are missing): `trace.txt`, `result.json`, `reaper.stderr`, `bt.txt`.
    Quote the evidence: host `0 loadapi missing=145` then `0 entry result=0` (no `register` lines at all), reaper layer `0 loadapi missing=1` then `0 entry result=0`, the `result.json` reason, and the `SWELL_dllMain` symbol-lookup line from `reaper.stderr` (red herring — the trace still shows `0 entry rec=non-null`).
    Line `Root cause:` — `REAPERAPI_LoadAPI` under `REAPERAPI_IMPLEMENT` (full header include in `plugin/src/main.cpp`) probes all 877 SDK names; REAPER 7.81's GetFunc lacks the name(s) in `docs/notes/oracle-missing-names.md` (host layer: 145, per `docs/notes/sdk-vs-reaper-api.md`), so `ReaperPluginEntry` returns 0 and registers nothing.
    Line `Fix:` — in `plugin/src/main.cpp`, define `REAPERAPI_MINIMAL` before including `reaper_plugin_functions.h` and add one `REAPERAPI_WANT_<name>` per REAPER function the plugin actually calls (applied in task 8).
    Keep the note under ~40 lines. Do not change plugin code in this task.
    Verify: `grep -q '^Root cause:' docs/notes/oracle-load-diagnosis.md && grep -q '^Fix:' docs/notes/oracle-load-diagnosis.md`

### 8. Fix the load failure

- Apply the `Fix:` from `docs/notes/oracle-load-diagnosis.md` (if it is the LoadAPI hypothesis: define `REAPERAPI_MINIMAL` and one `REAPERAPI_WANT_<name>` per REAPER function the plugin actually calls, before including `reaper_plugin_functions.h`; keep the "refuse to load" behavior only for those).
- Also fix the exit-139-on-quit if the trace/`bt.txt` shows it comes from the plugin's unload path; if it is not our code, record that in the diagnosis note and move on.
- Any function added later by a milestone must add its `REAPERAPI_WANT_` line in the same change; put that rule as a comment above the list.

**Verify**
```bash
tools/check && tools/reaper-oracle --layer all --scenario load
```

- [ ] Done

### 9. Behavior scenario: `jog_demo`

- Add `oracle/scenarios/jog_demo.lua` per design §5 (setup, run the action via `Main_OnCommand(NamedCommandLookup("_speed_reaper_jogdemo"), 0)`, sample the cursor each `defer` cycle for 3 s, the four checks with measured values in `detail`).
- If a check fails because of a plugin bug, fix the bug (smallest change; `plugin/src/jog_engine.*` or `main.cpp`); if it fails because the headless REAPER cannot run the timer or scrub, record the finding in `docs/notes/oracle-limits.md` and adjust the check to what *is* observable. Do not weaken thresholds without a written reason in that note.
- Add the scenario to `tools/reaper-oracle --layer all` (so a bare `tools/reaper-oracle` runs `load` then `jog_demo`).

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario jog_demo
```

- [ ] Done

### 10. Make the oracle part of `tools/check`

- In `tools/check`: after ctest, run `tools/reaper-oracle --layer host`; if env `ORACLE_FULL=1` run `tools/reaper-oracle` (all layers, all scenarios) instead. Keep `tools/check [ctest-regex]` behavior otherwise.
- Add `-DSPEED_REAPER_FAKE_DEVICE=ON` to the cmake configure line in `tools/check` and `tools/reaper-oracle` (the option is defined in `19-…`; an unused cmake variable only warns, which must not fail the build — verify).
- Document in `docs/plugin-oracle-design.md` a short "How to use" block with the three commands (`tools/check`, `ORACLE_FULL=1 tools/check`, `tools/reaper-oracle --layer reaper --scenario X`).

**Verify**
```bash
ORACLE_FULL=1 tools/check
```

- [ ] Done
