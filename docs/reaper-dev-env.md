# Research: REAPER C++ plugin development environment

Scope: what it takes to build a REAPER *plug-in extension* in C++ that drives
REAPER transport (playback cursor, shuttle/jog modes) from the Blackmagic
DaVinci Resolve Speed Editor. This doc records the researched requirements. They are implemented in
`flake.nix` + `.envrc` at the repo root (nix devShell — see §8).

## TL;DR

1. **REAPER extensions are native shared libraries** (`.so` / `.dll` / `.dylib`),
   not VSTs. They expose one entry point, `ReaperPluginEntry`, and are dropped
   into the REAPER resource path (`~/.config/REAPER/Plugins/` or
   `~/.config/REAPER/UserPlugins/` on Linux). REAPER scans both folders at
   startup.
2. **The SDK is a mini-SDK of headers** (`reaper_plugin.h` + a 641 KB
   `reaper_plugin_functions.h`), MIT-style licensed, free for commercial use.
   The de-facto standard source is the GitHub mirror
   [`justinfrankel/reaper-sdk`](https://github.com/justinfrankel/reaper-sdk)
   (the one the SWS extension vendors as a submodule).
3. **WDL (Cockos) is a hard dependency** — the SDK itself `#include`s
   `WDL/swell/swell.h` on non-Windows platforms (SWELL is the Win32
   compatibility layer; on Linux REAPER loads GDK into SWELL at runtime, so a
   SWELL-using plugin needs `gdk-3.0` headers at build time).
4. **Windows requires MSVC** (pure-virtual C++ interface classes → MSVC C++
   ABI; MinGW explicitly not supported, per the SDK header). Linux: any recent
   GCC (SWS builds with GCC 15/16 after small fixes); macOS: Apple clang.
5. **CMake is the cross-platform build system of choice** — used by the SWS
   extension and the `reaper-sdk-vscode` hello-world template. There is a
   nixpkgs derivation for SWS (`reaper-sws-extension`) whose input list is a
   ready-made recipe for our flake.
6. **The SDK ships no HID support.** The Speed Editor is a USB HID device; the
   plugin must bring its own device access (hidraw/libusb on Linux) and poll it
   from its own thread. The closest in-SDK prior art is the control surface
   plugin `reaper_csurf/` (Windows-oriented).

## 1. Anatomy of a REAPER extension

- A plug-in extension adds functionality to REAPER: external control surfaces,
  file-type import/export, audio services, actions, etc. (per the official
  [Extensions SDK page](https://www.reaper.fm/sdk/plugin/plugin.php)).
- Entry point: `ReaperPluginEntry` (`#define REAPER_PLUGIN_ENTRYPOINT
  ReaperPluginEntry` in `reaper_plugin.h`). On Linux it is exported with
  `__attribute__((visibility("default")))` and must be built with `-fPIC`
  (shared library).
- REAPER calls into the plugin with a large dispatch of function IDs
  (plugin name, registration, command handlers, …). A minimal "hello world"
  extension registers an action; invoking the action runs our code.
- Install locations (Linux, non-portable): `~/.config/REAPER/Plugins/` or
  `~/.config/REAPER/UserPlugins/` — both are scanned. Options → *Show REAPER
  resource path in explorer* reveals the path. SWS installs to
  `UserPlugins/`; naming the file `reaper_<name>.so` (no `lib` prefix) is the
  SWS convention (CMake `PREFIX ""`).
- After installing, restart REAPER; the plugin's actions appear in the
  Actions list.

## 2. The SDK

| Item | Detail |
| --- | --- |
| Official page | https://www.reaper.fm/sdk/plugin/plugin.php ("source … now available via GitHub") |
| De-facto repo | https://github.com/justinfrankel/reaper-sdk (mirrors the official mini-SDK; vendored by SWS as `vendor/reaper-sdk`) |
| License | MIT-style (permissive, commercial OK): no warranty; keep origin notice; altered versions marked as such |
| `sdk/reaper_plugin.h` | Core header: types, `ReaPluginInfo`, entry point, platform notes (~73 KB). Copyright 2006–2015 Cockos but the repo tracks current API |
| `sdk/reaper_plugin_functions.h` | The full C API surface (~641 KB single header — large; a token-efficient extraction is a separate task) |
| `reaper-plugins/reaper_csurf/` | **Control surface plugin source** — external surface → REAPER. Closest architectural prior art to us (MIDI/OSC/HID-ish surfaces, Windows-oriented, uses `windows.h`) |
| `sdk/example_m3u/`, `sdk/example_raw/` | Tiny example extensions (import filter / raw PCM) |
| `reaper-plugins/reaper_plugin.h` | Copy of the SDK header at repo root |

**WDL dependency** (README of the SDK repo): "Paths that should be added in
order to compile: `WDL/`" — merge or symlink
[`justinfrankel/WDL`](https://github.com/justinfrankel/WDL) (Cockos WDL, same
permissive license). `reaper_plugin.h` requires `WDL/swell/swell.h` on
non-Windows, so WDL is not optional.

In this repo the WDL submodule sits at `vendor/WDL` and contains the nested
`WDL/` mirror, so the WDL include root is **`vendor/WDL/WDL`** —
`reaper_plugin.h`'s relative `#include "../WDL/swell/swell.h"` resolves
against that root (verified: `-Ivendor/WDL/WDL` works; plain `-Ivendor/WDL`
resolves to `vendor/WDL/swell/swell.h`, which does not exist).

## 3. Compiler / toolchain requirements per platform

| Platform | Compiler | Notes |
| --- | --- | --- |
| Windows | **MSVC required** (VS 2019/2022 + Windows SDK) | SDK header: "Extensions for REAPER/win32 should be written in C++ and compiled using MSVC … the C++ ABI must be compatible — sorry, mingw users". SWS: MSVC `/EHsc`, PCH, `/OPT:REF` in Release |
| Linux | GCC (recent; SWS fixed for GCC 16 in nixpkgs), any CMake-supported | `sudo apt install build-essential cmake gdb git valgrind` (per reaper-sdk-vscode template). SWELL on Linux: REAPER loads GDK at runtime via `SWELL_WANT_LOAD_LIBRARY`, so SWELL-using code needs `gdk-3.0` headers (pkg-config) but does **not** link GDK |
| macOS | Apple clang / Xcode (CLT sufficient: `xcode-select --install`) | SWS supports `CMAKE_OSX_ARCHITECTURES` (x86_64/arm64) and deployment target |
| C++ standard | C++11 minimum (SWS requirement); C++17 in the nixpkgs SWS derivation | Plain C or C++ both work; most community code is C++ |

## 4. Build system

- **CMake 3.13.3+** with either Unix Makefiles or Ninja. (SWS wiki; the
  reaper-sdk-vscode template uses CMake presets + VSCode CMake Tools.)
- SWS is the reference implementation of a serious cross-platform CMake build:
  - submodules: `vendor/WDL`, `vendor/reaper-sdk` (justinfrankel mirror),
    `vendor/taglib`
  - `find_package(WDL|SWELL|LICE|JNetLib|TagLib)`; on Linux `pkg_search_module(GDK gdk-3.0)`
  - install target: `LIBRARY DESTINATION UserPlugins` under the REAPER
    resource prefix (Linux `~/.config/REAPER`, macOS `~/Library/Application Support/REAPER`, Windows `%APPDATA%\REAPER`)
  - nixpkgs derivation `reaper-sws-extension` (`pkgs/by-name/re/reaper-sws-extension/linux.nix`):
    `nativeBuildInputs = [ cmake git perl php pkg-config ]`, `buildInputs = [ gtk3 ]`,
    `strictDeps = true`, `CMAKE_CXX_STANDARD 17`. **This is the checklist for our flake**
    (we can drop perl/php/taglib if we don't need SWS's codegen).
- Minimal hello-world alternative: [`reaper-sdk-vscode`
  template](https://github.com/halbius/reaper-sdk-vscode) (VSCode + CMake
  presets, "developed and tested on Windows 10, macOS, Ubuntu 20.04") — a good
  starting skeleton for our first plugin; its `source/main.cpp` shows the
  entry-point boilerplate.

## 5. Prior art / references

| Project | Why it matters |
| --- | --- |
| [SWS / S&M extension](https://github.com/reaper-oss/sws) (open source, MIT-style) | The canonical "how to do useful things with the SDK" reference; huge action library; CMake; nixpkgs-packaged. Control surface support (`sws_reacolor` etc.) shows how external input is handled |
| `reaper_csurf` (in SDK repo) | Official control-surface plugin: how REAPER models external surfaces (CSurf API: transport state, jog/shuttle, faders) |
| `reaper-sdk-vscode` template | Minimal cross-platform CMake hello-world extension |
| Local: `docs/reaper-api.htm` | ReaScript API reference, **generated by REAPER v7.82/win64** — i.e. we target the REAPER 7.x SDK generation. (This is the ReaScript doc, not the C header — useful for API semantics, e.g. transport/`MovePlayCursor`) |
| Sibling project `~/projects/speed-audacity` | Same Speed Editor hardware, Audacity side; will share the USB/HID access approach |

## 6. REAPER runtime for testing

- REAPER 7.82 is current (Oct 2026). Free evaluation (30 days, unlimited
  nag) or registered license; the Linux build works on x86_64.
- Testing loop: `cmake --build build --target install` → restart REAPER →
  Actions list shows the plugin → invoke action → check REAPER console /
  on-screen transport.
- Debugging: `gdb` (attach to `reaper`), or launch REAPER under gdb; SWS and
  the template document VSCode `launch.json` with `"program": path/to/reaper`.
- The REAPER *resource path* (not install path) is where plugins go; for a
  portable install it is `REAPER/` next to the binary.

## 7. Hardware: Speed Editor requirements

- The Blackmagic Speed Editor is a **USB HID** device (not MIDI) — the SDK
  gives no device access; the plugin must open it itself.
- Linux: device appears as `/dev/hidrawN` (and/or raw USB). Requirements:
  - user in `input` group **or** a udev rule granting read access to the
    device's VID/PID (verify with `lsusb` / `ls -l /dev/hidraw*`)
  - candidate stacks: `libhidapi` (hidraw backend), raw `hidraw` + `libudev`
    for hotplug, or `libusb`
  - the plugin must poll the device from its own worker thread (Reaper UI
    thread must not block) and translate: jog-wheel increments → move
    playback cursor; shuttle/jog/scroll buttons → transport mode changes.
  - Headless testability: a plain-C/CLI tool reading the same device first
    (mirrors the `speed-audacity` plan) de-risks HID before the plugin exists.
- Windows: Win32 HID API (`hid.dll`) — `reaper_csurf` demonstrates the
  Windows-side pattern if we get there.

## 8. This machine + dev env (implemented)

Facts about the dev box (verified 2026-10):

- Ubuntu 24.04.4 LTS, Nix 2.34.6 active; a nix env with **GCC 15.2.0** and
  GNU Make 4.4.1 is already in the default PATH here.
- House style for nix projects (`~/projects/*.nix`, envrc) is **matched**:
  `.envrc` is the standard nix-direnv 2.2.1 template (byte-identical to
  `tone`, `local-llm`, `deployment`, `tempo-trainer`, `kyre`); `flake.nix`
  follows the `~/projects/tone/flake.nix` shape (inputs `nixpkgs` +
  `numtide/flake-utils`, `eachSystem ["x86_64-linux"]`,
  `import nixpkgs { config.allowUnfree = true }`, `in rec { devShell = ... }`).

Requirements the dev env must satisfy (from §3–§7):

1. **Build tools**: `cmake` (≥3.13.3), `ninja`, `g++` (GCC 13+ fine),
   `pkg-config`, `git`; `gdb` + `valgrind` for debugging.
2. **SDK sources** as submodules/vendored: `justinfrankel/reaper-sdk`,
   `justinfrankel/WDL` (SWS does exactly this).
3. **SWELL/GDK**: `gtk3` (for `gdk-3.0` pkg-config headers) — only if our
   plugin touches SWELL UI; a headless transport-control plugin may not need
   it initially, but SWELL types appear in the SDK header path, so keep gtk3
   in the devshell to be safe.
4. **HID**: `libhidapi` + `libusb` (runtime libs + headers), `udev` for rules.
5. **REAPER binary** for manual testing: nixpkgs has a `reaper` package
   (unfree); otherwise manual install. (Verify availability in next task.)
6. **C++ standard**: build with `-std=c++17` (matches current nixpkgs SWS
   practice, GCC 15 default territory).

### 8.1 Verified working (2026-10, in `nix develop`)

- `nix develop` (or direnv via `.envrc`) provides: GCC 16.2.0, CMake 4.4.3,
  Ninja 1.13.2, pkg-config, git, gdb, valgrind; pkg-config modules
  `gdk-3.0` 3.24.52, `hidapi-hidraw` 0.15.0, `libusb-1.0` 1.0.30, `udev`
  261; `reaper` 7.81 on PATH (req 5: nixpkgs `reaper` works).
- End-to-end smoke test: a minimal extension (classic entry-point pattern
  from `sdk/example_raw`) compiles and exports `ReaperPluginEntry`:
  ```sh
  g++ -std=c++17 -fPIC -shared \
      -Ivendor/reaper-sdk/sdk -Ivendor/WDL/WDL \
      $(pkg-config --cflags gdk-3.0) \
      hello.cpp -o hello.so
  nm -D hello.so | grep -w ReaperPluginEntry    # → T ReaperPluginEntry
  ```
  Include-root gotchas (all verified):
  - WDL root is `-Ivendor/WDL/WDL` (nested mirror; §2), not `-Ivendor/WDL`.
  - `pkg-config --cflags gdk-3.0` is required on Linux: SWELL's
    `swell-internal.h` includes `<gdk/gdk.h>` (header-only; GDK is not
    linked).
  - A bare entry point must not call `REAPERAPI_LoadAPI` — that lives in
    `reaper_plugin_functions.h` under `#ifdef REAPERAPI_IMPLEMENT`.

## Sources

- Official Extensions SDK page: https://www.reaper.fm/sdk/plugin/plugin.php
- SDK mirror: https://github.com/justinfrankel/reaper-sdk (+ `sdk/LICENSE`, `README`, `sdk/reaper_plugin.h`)
- WDL: https://github.com/justinfrankel/WDL (https://www.cockos.com/wdl/)
- SWS extension: https://github.com/reaper-oss/sws — build wiki
  (https://github.com/reaper-oss/sws/wiki/Building-the-SWS-Extension),
  `CMakeLists.txt`, `.gitmodules`
- nixpkgs `reaper-sws-extension`: `pkgs/by-name/re/reaper-sws-extension/linux.nix`
  (GCC-16 build fix: https://github.com/NixOS/nixpkgs/pull/570294)
- reaper-sdk-vscode template: https://github.com/halbius/reaper-sdk-vscode
- UserPlugins folder: SWS end-user install wiki + reapertips.com install guide
