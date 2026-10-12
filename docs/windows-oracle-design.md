# Windows plugin oracle

Question: *does `reaper_speed_reaper.dll` work in Windows REAPER?* Compiling
is not evidence — the first hand test failed with "error failed load" because
the DLL imported `libmcfgthread-2.dll` (std::thread in GCC 16 MinGW), absent on
Windows. Fix: `-static` link. The oracle exists so this class of bug is caught
without a Windows box.

## Layers (cheapest first; each strictly stronger)
**S — static PE checks** (`tools/windows-oracle`, run by `tools/check`)
- S1 PE32+ x86-64 DLL; S2 imports ⊆ allowlist of always-present system DLLs;
  S3 exports `ReaperPluginEntry`; S4 filename `reaper_*.dll`.
- Predicts every "failed load" cause except a runtime crash in DllMain/Entry.

**W — wine load + behaviour** (`--layer wine`, TODO)
- Cross-build `plugin_host.exe` (port `tools/plugin_host.cpp`: `dlfcn` →
  `LoadLibrary/GetProcAddress`, behind `#ifdef _WIN32`).
- Under `wine64` (headless, `WINEDEBUG=-all`, throwaway `WINEPREFIX`): load the
  DLL, call `ReaperPluginEntry` with the fake `reaper_plugin_info_t`, run the
  existing `hid_wheel` scenario. HID is the fake device (needs
  `SPEED_REAPER_FAKE_DEVICE=ON` in a test-flavoured Windows build, since wine
  has no Speed Editor). Assert the same trace as the Linux oracle.
- Catches: unresolved imports by ordinal/name, static-init crashes, thread
  start/stop failures, hidapi LoadLibrary(hid.dll) path.

**H — hand test on real Windows REAPER** (SMOKE-TEST.md)
- Not automatable; run after S+W pass. Checks Actions list entry, real wheel.
  Copy: `cp result/reaper/reaper_speed_reaper.dll /mnt/c/.../UserPlugins/`
  (REAPER must be closed; it locks loaded DLLs).

## Wiring
`tools/check` runs layer S on every invocation (SKIP_WINDOWS=1 skips);
layer W joins when ORACLE_FULL=1 once implemented.
