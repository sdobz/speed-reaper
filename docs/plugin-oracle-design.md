# Oracle design: how an agent verifies the plugin without a human

Normative for `vibes/18-oracle-linux.md` and `vibes/19-oracle-device-windows.md`;
referenced by every later milestone's Verify. Goal: **no human in the debug loop** —
the agent edits, runs one command, reads machine-readable artifacts, repeats. Humans
only run the final real-hardware hand test after the oracle is green.

Evidence this is feasible (checked on the dev box): REAPER 7.81 (`nixpkgs#reaper`,
unfree; flake already allows it) starts under `xvfb-run reaper -newinst -nosplash`
with an isolated `HOME`; a Lua file passed on the command line runs inside REAPER
and can write files, call `NamedCommandLookup`, `Main_OnCommand`, etc.

## How to use

```
tools/check                                   # build + unit tests + oracle host layer
ORACLE_FULL=1 tools/check                     # same, but oracle runs all layers, all scenarios
tools/reaper-oracle --layer reaper --scenario jog_demo   # one reaper-layer scenario only
```

`ORACLE_FULL=1 tools/check` is the heavy end-to-end check (~1 min: REAPER starts
per scenario). `tools/check` alone stays fast (<30 s incl. build) and catches
link/entry regressions.

## §1 The command

```
tools/reaper-oracle [--layer host|reaper|all] [--scenario NAME] [--platform linux|windows] [--report-only]
```

- Prints one final line: `ORACLE PASS <layer>[/<scenario>]` or `ORACLE FAIL <layer>[/<scenario>]: <reason>`; exit 0 on PASS.
- `--report-only`: exit 0 whenever the *harness itself* worked (artifacts written), even if the verdict is FAIL. Used by tasks that build the oracle before the plugin passes it.
- Artifacts, always, in `build/oracle/`: `result.json`, `trace.txt`, `reaper.stderr`, `probe.json` (reaper layer), `bt.txt` (only on crash/timeout). `result.json`: `{"layer","scenario","pass","reason","checks":[{"name","pass","detail"}]}`.
- Layers: `host` = `plugin_host` (no GUI, <2 s); `reaper` = real REAPER headless (~15–40 s); `all` = both. Default `all`.
- Every run uses an isolated `HOME=build/oracle/home` (wiped each run) so no state leaks between runs or into the user's real REAPER config.

## §2 Plugin trace (`plugin/src/trace.h/.cpp`, pure)

`void Trace(const char *fmt, ...)` — if env `SPEED_REAPER_TRACE` is set, append one line
to that file and `fflush` (so a crash loses nothing); otherwise no-op. Safe from any thread.
Line format: `<ms since first trace> <message>`.

Each call opens, appends, flushes and **closes** the file (never leak the
`FILE*`: the Windows CRT allows only 512 open streams, which once truncated a
hand-test trace at 509 lines). An empty `SPEED_REAPER_TRACE` is the same as unset.

**Tracing a hand test on Windows** (real REAPER, plugin built for Windows):

- Enable (persistent, per user): `setx SPEED_REAPER_TRACE C:\sr_trace.txt`, then fully quit and restart
  REAPER (only processes started afterwards see it). The file is appended to across runs; delete it
  first for a clean capture, and quit REAPER before copying it. From WSL it is `/mnt/c/sr_trace.txt`.
- **Disable / undo `setx`**: `setx` cannot delete a variable (`setx SPEED_REAPER_TRACE ""` only makes it
  empty, which `Trace` treats as off but leaves it defined). To remove it:
  `reg delete HKCU\Environment /v SPEED_REAPER_TRACE /f` (cmd), or in PowerShell
  `[Environment]::SetEnvironmentVariable("SPEED_REAPER_TRACE", $null, "User")`.
  Then restart REAPER. The current shell keeps its old copy until reopened (`set SPEED_REAPER_TRACE=` clears it there).
- Only lifecycle/registration, timer-gap and `log …` lines are traced; per-report wheel diagnostics
  were removed after M1 (findings: `docs/notes/m1-wheel-scale.md`).

Required messages from `ReaperPluginEntry` (exact prefixes; oracle greps them):
`entry rec=<null|non-null>`, `loadapi missing=<n> [names...]`, `register <what> ok|FAIL`,
`action <id_str> cmd=<n>`, `entry result=<0|1>`, `unload`. Later milestones add:
`cmd <n>` (every `RunCommand`), `mode <Jog|Scrl|Shtl>`, `led jog=<bits> main=<hex>`,
`log <line>` (every HID log line). **Rule: anything an oracle scenario must observe
that is not visible through REAPER's API gets a trace line.**

Test hook: env `SPEED_REAPER_TEST_CRASH=entry` makes `ReaperPluginEntry` call
`std::abort()` right after tracing `entry rec=…` (proves crash capture works).

## §3 `plugin_host` (`plugin/tools/plugin_host.cpp`, target `plugin_host`)

Stand-in for REAPER with no GUI: `plugin_host <plugin.so|.dll> [--ticks N] [--expect-entry 0|1] [--expect-actions id1,id2] [--report-only]`.
1. Load the plugin (`dlopen` / `LoadLibrary`); print `loaded` or the loader error.
2. Build a fake `reaper_plugin_info_t`: `GetFunc(name)` returns a generic non-null stub **iff
   `name` is in `oracle/reaper_api_names.txt`** (the functions real REAPER 7.81 exports), else
   `nullptr` — this reproduces real REAPER's behavior for missing API functions.
   `Register(name, ptr)` records calls; `"custom_action"` registrations get ids 40001+; `"timer"` callbacks are kept.
3. Call `ReaperPluginEntry(inst, &rec)`; print `entry=<n> registered=<list>`.
4. Run each recorded timer callback `--ticks` times (default 50), then call `ReaperPluginEntry(inst, nullptr)` (unload).
5. Exit 0 if no crash and expectations (if any) match; `--report-only`: exit 0 unless the host itself failed.

## §4 Real-REAPER layer

`tools/reaper-oracle` (bash): ensure `build/plugin/reaper_speed_reaper.so` is built; wipe
`build/oracle/home`; copy the plugin to `$HOME/.config/REAPER/UserPlugins/`; run
`timeout 60 xvfb-run -a reaper -newinst -nosplash oracle/scenarios/<scenario>.lua` with
`SPEED_REAPER_TRACE=build/oracle/trace.txt`. Exit status ≥128 or timeout → crash capture:
re-run under `gdb -batch -ex run -ex bt --args …` into `bt.txt`, plus `LD_DEBUG=libs` filtered to `speed_reaper`
into `reaper.stderr` (shows whether REAPER even attempted `dlopen` and why it failed).

Per-scenario environment: if `oracle/scenarios/NAME.env` exists the harness `source`s it before launching REAPER (e.g. `export SPEED_REAPER_FAKE_DEVICE=oracle/fake/wheel_turn.txt`).

Scenarios are Lua in `oracle/scenarios/NAME.lua`, all starting with
`dofile(os.getenv("ORACLE_LIB"))` (harness sets it to `oracle/lib.lua`). `lib.lua` API:
`check(name, cond, detail)`, `wait_ms(ms, fn)` (runs `reaper.defer` until the time elapsed,
calling `fn()` each cycle), `finish()` (writes `probe.json` to `$ORACLE_OUT`, then quits REAPER with
`Main_OnCommand(40004, 0)`). The harness derives `result.json` from `probe.json` and the exit code.
A scenario that never calls `finish()` within 60 s is a FAIL (`reason: scenario timeout`).

Scenario catalogue (grows by milestone): `load` (every plugin action id resolves with
`NamedCommandLookup("_<id_str>")` ≠ 0), `jog_demo` (§5), later `hid_wheel`, `buttons`, `verbs`, `spike_rate`.

## §5 Behavior checks (`jog_demo`)

Setup: empty project, `SetEditCurPos(10, false, false)`, transport stopped. Run the jog-demo
action, sample `GetCursorPosition()` every `defer` cycle for 3 s. Checks:
final − start in [0.3, 0.8] s (design ≈ 0.5 s); never decreases; max step between
consecutive samples < 0.05 s (the "no stutter" check); last 0.5 s of samples are constant (coasts to a stop).

## §6 Fake device (`vibes/19-oracle-device-windows.md`)

`IHidTransport` (`src/speed_editor/hid_transport.h`) is the seam under `SpeedEditorHID`:
`bool Open(const std::string &serial)`, `void Close()`, `int Read(uint8_t *buf, size_t len, int timeout_ms)`
(>0 bytes, 0 timeout, −1 error/disconnect), `int Write(const uint8_t*, size_t)`,
`int SendFeature(const uint8_t*, size_t)`, `int GetFeature(uint8_t id, uint8_t *buf, size_t len)`,
`std::string Serial() const`. `HidapiTransport` is the real one (unchanged behavior).
`FakeTransport` (`src/speed_editor/fake_transport.{h,cpp}`) plays a script file and implements the
device side of the auth handshake (the algorithm is in `hid_device.cpp`).
Script lines (ms from `Open`): `at 500 wheel 30` · `at 600 key 0x3C down` · `at 700 key 0x3C up` ·
`at 2000 unplug` · `at 3000 plug`. `# comments`, blank lines ignored. Selected by env
`SPEED_REAPER_FAKE_DEVICE=<script>` **only when built with `-DSPEED_REAPER_FAKE_DEVICE=ON`**
(CMake option, ON in `tools/check`/oracle builds, OFF otherwise). Scripts live in `oracle/fake/*.txt`.

## §7 Windows (best effort, layered)

`--platform windows`: (a) static `x86_64-w64-mingw32-objdump -p` export/import check of the DLL; (b) `plugin_host.exe`
(mingw build, same code) under `wine64` loading the DLL; (c) real Windows REAPER under wine + Xvfb is a **spike**
(may be flaky; documented, not required for green). (a)+(b) are required.
