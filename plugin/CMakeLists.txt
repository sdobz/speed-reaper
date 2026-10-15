# CMake build for the "Speed Reaper" REAPER extension (vibes/05-prototypes.md
# task 6). REAPER extensions are plain shared libraries with one entry point
# (ReaperPluginEntry); see docs/reaper-dev-env.md §1.
#
# Usage (from the nix dev env — `direnv allow` / `nix develop`):
#   cmake -S plugin -B build/plugin -G Ninja
#   cmake --build build/plugin
#   → build/plugin/reaper_speed_reaper.so
#
# Install for testing (see SMOKE-TEST.md):
#   cp build/plugin/reaper_speed_reaper.so ~/.config/REAPER/UserPlugins/
#
# Cross-platform: builds on Linux (native .so) and Windows (cross .dll via
# nixpkgs' mingwW64 toolchain, see CROSS-PLATFORM.md at the repo root and
# flake.nix). The SDK README says Windows extensions need MSVC, but that
# constraint only bites when REAPER C++ interface objects cross the plugin
# boundary; this extension's entire surface to REAPER is C-ABI
# (ReaperPluginEntry + function-pointer calls), so MinGW works.
# On Windows the output is reaper_speed_reaper.dll, installed like the .so.

cmake_minimum_required(VERSION 3.13)
project(speed_reaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

# Transport selection (docs/plugin-oracle-design.md §6, vibes/19 task 3):
# test builds only. When ON, the plugin, hid_probe and the unit tests get the
# SPEED_REAPER_FAKE_DEVICE macro, so MakeTransport()
# (src/transport_factory.cpp) returns a FakeTransport when the env var of the
# same name names a script. Default OFF: production builds always use
# HidapiTransport and no env lookup is compiled in.
option(SPEED_REAPER_FAKE_DEVICE "Select the Speed Editor transport from the SPEED_REAPER_FAKE_DEVICE env var (test builds only)" OFF)
add_compile_definitions($<$<BOOL:${SPEED_REAPER_FAKE_DEVICE}>:SPEED_REAPER_FAKE_DEVICE>)

if(WIN32 AND NOT MSVC)
  # MinGW path (see CROSS-PLATFORM.md): fine as long as this extension
  # implements none of REAPER's C++ interface classes. If that changes,
  # restore the hard MSVC-only check.
  message(STATUS "Non-MSVC Windows build: C-ABI-only REAPER surface assumed "
                 "(no C++ objects cross the plugin boundary). See CROSS-PLATFORM.md.")
endif()

# REAPER-free sources: linked into both the plugin and the unit tests
# (plugin/test/). Add new pure-logic .cpp files here, never to the
# plugin-only list below.
set(PLUGIN_PURE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/button_diff.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/gesture.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hold_nudge.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/led_state.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/nudge.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/router.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/shuttle.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/target.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/trace.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/verbs.cpp
)

get_filename_component(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
if(UNIX AND NOT APPLE OR WIN32)
  # Speed Editor driver + HID wheel source (docs/plugin-design-m1-m3.md §1, §4):
  # REAPER-free, so they belong in PLUGIN_PURE_SOURCES (linked into the plugin
  # and every unit test). M1–M4 are Linux-only; the Windows cross build keeps
  # the pre-HID source list.
  list(APPEND PLUGIN_PURE_SOURCES
    ${REPO_ROOT}/src/speed_editor/hid_device.cpp
    ${REPO_ROOT}/src/speed_editor/hidapi_transport.cpp
    ${REPO_ROOT}/src/speed_editor/fake_transport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hid_wheel_source.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport_factory.cpp
  )
endif()

add_library(speed_reaper SHARED
  src/main.cpp
  # REAPER-backed IProject (M5 design §2): needs the REAPER host API, so it
  # stays OUT of PLUGIN_PURE_SOURCES (the unit tests use fake_project.h).
  src/reaper_project.cpp
  # REAPER-backed IPlayheadSink/IActionSink (40-cleanup §4): needs the REAPER
  # host API, so it stays OUT of PLUGIN_PURE_SOURCES like reaper_project.cpp.
  src/reaper_sinks.cpp
  ${PLUGIN_PURE_SOURCES}
)

# Vendored SDK + WDL (SWELL include root is the nested WDL/WDL — see
# docs/reaper-dev-env.md §2 for the gotcha). SYSTEM: keep -Wall -Wextra from
# firing on the SDK/WDL headers (they are not ours to fix).
target_include_directories(speed_reaper PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(speed_reaper SYSTEM PRIVATE
  ${REPO_ROOT}/vendor/reaper-sdk/sdk
  ${REPO_ROOT}/vendor/WDL/WDL
)

if(WIN32)
  # Static hidapi winapi backend (HIDAPI_PREFIX = its install dir, passed by
  # flake.nix; same layout src/Makefile CROSS=1 uses). It LoadLibrary()s the
  # system hid.dll at runtime, and needs setupapi for enumeration.
  set(HIDAPI_PREFIX "" CACHE PATH "Windows hidapi install prefix")
  target_include_directories(speed_reaper PRIVATE ${REPO_ROOT}/src)
  target_include_directories(speed_reaper SYSTEM PRIVATE ${HIDAPI_PREFIX}/include)
  target_link_directories(speed_reaper PRIVATE ${HIDAPI_PREFIX}/lib)
  target_link_libraries(speed_reaper PRIVATE hidapi setupapi)
endif()

if(MINGW)
  # Self-contained .dll: no libstdc++/libgcc dependency on the target.
  target_link_options(speed_reaper PRIVATE -static -static-libstdc++ -static-libgcc)
endif()

if(UNIX AND NOT APPLE)
  # SWELL on Linux resolves gdk-3.0 via pkg-config at build time (header-only;
  # REAPER loads GDK into SWELL at runtime, so nothing is linked).
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GDK3 REQUIRED gdk-3.0)
  target_include_directories(speed_reaper SYSTEM PRIVATE ${GDK3_INCLUDE_DIRS})

  # Speed Editor wheel driver: hidapi hidraw backend — the same module
  # src/Makefile uses for speedcli. SYSTEM: keep -Wall -Wextra from firing on
  # hidapi's headers (not ours to fix); our code stays warning-free.
  pkg_check_modules(HIDAPI REQUIRED IMPORTED_TARGET hidapi-hidraw)
  target_include_directories(speed_reaper PRIVATE ${REPO_ROOT}/src)
  target_include_directories(speed_reaper SYSTEM PRIVATE ${HIDAPI_INCLUDE_DIRS})
  target_link_libraries(speed_reaper PRIVATE PkgConfig::HIDAPI)

  # Probe tool (docs/plugin-design-m1-m3.md §5): standalone HID exerciser,
  # no REAPER. Links the same sources as the plugin minus main.cpp
  # (= PLUGIN_PURE_SOURCES). Output: build/plugin/hid_probe.
  add_executable(hid_probe tools/hid_probe.cpp ${PLUGIN_PURE_SOURCES})
  target_include_directories(hid_probe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${REPO_ROOT}/src
  )
  target_include_directories(hid_probe SYSTEM PRIVATE ${HIDAPI_INCLUDE_DIRS})
  target_link_libraries(hid_probe PRIVATE PkgConfig::HIDAPI)
endif()

# Guarded: cross builds (flake) may pack a tree that predates test/. After the
# UNIX block above so the tests can see REPO_ROOT and PkgConfig::HIDAPI.
# Skipped when cross-compiling: the tests need the Linux-only HID sources and
# POSIX APIs (setenv), and a Windows binary can't run in the build sandbox.
if(NOT CMAKE_CROSSCOMPILING AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt)
  enable_testing()
  add_subdirectory(test)
endif()

# Oracle host (docs/plugin-oracle-design.md §3): stand-in for REAPER with no
# GUI; loads the built plugin and fakes reaper_plugin_info_t. Guarded like
# test/ above (cross builds may pack an older tree).
if(NOT WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/plugin_host.cpp)
  add_executable(plugin_host tools/plugin_host.cpp)
  if(UNIX AND NOT APPLE)
    target_link_libraries(plugin_host PRIVATE dl)
  endif()
endif()

# SWS convention: reaper_<name>.so (no lib prefix).
set_target_properties(speed_reaper PROPERTIES
  PREFIX ""
  OUTPUT_NAME "reaper_speed_reaper"
)
