# Cross-platform build matrix (Linux / Windows)

This is a **cross-platform project**: every artifact builds for both
x86-64 Linux (native) and x86-64 Windows (cross-compiled from the same
machine with nixpkgs' `pkgsCross.mingwW64`, triple `x86_64-w64-mingw32`).
Future agents: do not treat Windows as out of scope — new code should
build on both targets via the flake before a task is done.

| Artifact | Source | Linux | Windows (cross) |
|---|---|---|---|
| `speedcli` (Speed Editor control CLI) | `src/` + `src/Makefile` | `nix build .#packages.x86_64-linux.speedcli` → `bin/speedcli` (ELF, shared `libhidapi-hidraw`) | `nix build .#packages.x86_64-pc-windows-gnu.speedcli` → `bin/speedcli.exe` (PE32+, static winapi `hidapi`, self-contained) |
| REAPER plugin `reaper_speed_reaper` (smooth playhead jog demo) | `plugin/` + `plugin/CMakeLists.txt` | dev shell: `cmake -S plugin -B build/plugin -G Ninja && cmake --build build/plugin` → `build/plugin/reaper_speed_reaper.so` | `nix build .#reaper-plugin-windows` → `reaper/reaper_speed_reaper.dll` (PE32+ DLL, static C++ runtime) |

Convenience aliases: `.#speedcli-windows` and `.#reaper-plugin-windows`
resolve to the Windows builds from Linux. `defaultPackage` per system is
`speedcli`.

## Gotchas (learned the hard way)

- **Target triple:** the standard mingw triple is `x86_64-w64-mingw32`
  (`pkgsCross.mingwW64`). The non-standard `x86_64-w64-windows-gnu` is
  rejected by `config.sub` — do not use it.
- **REAPER plugin + MinGW:** the SDK README says Windows extensions need
  MSVC ("sorry, mingw users"). That constraint only bites when **C++
  interface objects cross the plugin boundary** (e.g. implementing a
  REAPER `IContext`/`IEditSurface` with a vtable REAPER instantiates).
  This extension's entire REAPER surface is C-ABI (`ReaperPluginEntry`
  + function-pointer calls like `hookcommand2`/`CSurf_ScrubAmt`), and all
  C++ classes (`IWheelSource`, `JogEngine`, ...) are internal, so the
  MinGW cross-build works. If the plugin ever starts implementing REAPER
  C++ interface classes, the MSVC-only path returns — restore the
  `FATAL_ERROR` in `plugin/CMakeLists.txt` in that case.
- **hidapi backend per platform:** Linux links nixpkgs' shared `hidapi`
  (hidraw backend, udev + `/dev/hidraw*`); Windows cross-builds link a
  static winapi `hidapi` that `LoadLibrary`'s the system `hid.dll` at
  runtime. No code changes needed; the Makefile selects the link line via
  `CROSS=1` / `HIDAPI_PREFIX`.
- **SWELL:** on Linux the plugin needs `gdk-3.0` headers at build time
  (REAPER loads GDK into SWELL at runtime; nothing is linked). On Windows
  SWELL is the native Win32 API — no extra deps. CMake handles both.
- **Vendor sources must be plain tracked files, NOT submodules:**
  flake sources are filtered git trees — submodule gitlinks are packed as
  *nothing*, so `vendor/reaper-sdk` + `vendor/WDL` used to vanish from the
  nix build (dev-shell builds read the working tree and hid this). They are
  now regular files in the repo (provenance: reaper-sdk @ c0eafe8, WDL @
  d30c30b — keep a note when you update them). Do not re-submodule them.
- **`nix flake check` meta:** for cross derivations, `meta.platforms` must
  contain both the triple spelling and the elaborated target spelling
  (e.g. `x86_64-pc-windows-gnu` and `x86_64-windows`) or check-meta fails.
  `crossPlatforms` lists the build host.
- **Installing the Windows artifacts:** `speedcli.exe` runs anywhere;
  `reaper_speed_reaper.dll` goes to REAPER's resource `UserPlugins/`
  (`%APPDATA%\REAPER\UserPlugins\`) — same flow as the `.so`, see
  `SMOKE-TEST.md`.
