# Oracle load-diagnosis — REAPER 7.81 (load scenario)

Evidence from `tools/reaper-oracle --report-only` (`build/oracle/`, fresh run, REAPER 7.81/linux-x86_64):

`trace.txt` — host layer (plugin_host):

```
0 entry rec=non-null
0 loadapi missing=145
0 entry result=0
```

No `register` lines at all — entry returned before registering any action
(`plugin_host: expected entry=1, got 0`; actions `speed_reaper_jogdemo`,
`speed_reaper_scrub_fwd`, `speed_reaper_scrub_back` all unregistered).

`trace.txt` — reaper layer (real REAPER):

```
0 entry rec=non-null
0 loadapi missing=1
0 entry result=0
```

`result.json`:

```
"pass": false,
"reason": "host: entry/actions expectations not met (plugin_host exit 1); reaper: check(s) failed: action speed_reaper_jogdemo, action speed_reaper_scrub_fwd, action speed_reaper_scrub_back"
```

`reaper.stderr` (red herring — the trace still shows `0 entry rec=non-null`,
so REAPER does reach our entry point):

```
reaper_speed_reaper.so: error: symbol lookup error: undefined symbol: SWELL_dllMain (fatal)
```

(`bt.txt` is the gdb backtrace of an unrelated SIGSEGV in REAPER's own
`.reaper-wrapped` binary during the session; it carries no plugin frames.)

Root cause: `REAPERAPI_LoadAPI` under `REAPERAPI_IMPLEMENT` (full header include in `plugin/src/main.cpp`) probes all 877 SDK names; REAPER 7.81's GetFunc lacks `EnumThemeColors` (the 1 missing name in `docs/notes/oracle-missing-names.md`), and the host layer lacks the 145 names listed in `docs/notes/sdk-vs-reaper-api.md`, so `ReaperPluginEntry` returns 0 and registers nothing.

Fix: in `plugin/src/main.cpp`, define `REAPERAPI_MINIMAL` before including `reaper_plugin_functions.h` and add one `REAPERAPI_WANT_<name>` per REAPER function the plugin actually calls (applied in task 8).
