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
  ${CMAKE_CURRENT_SOURCE_DIR}/src/jog_engine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/trace.cpp
)

add_library(speed_reaper SHARED
  src/main.cpp
  ${PLUGIN_PURE_SOURCES}
)

# Guarded: cross builds (flake) may pack a tree that predates test/.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/CMakeLists.txt)
  enable_testing()
  add_subdirectory(test)
endif()

# SWS convention: reaper_<name>.so (no lib prefix).
set_target_properties(speed_reaper PROPERTIES
  PREFIX ""
  OUTPUT_NAME "reaper_speed_reaper"
)

# Vendored SDK + WDL (SWELL include root is the nested WDL/WDL — see
# docs/reaper-dev-env.md §2 for the gotcha). SYSTEM: keep -Wall -Wextra from
# firing on the SDK/WDL headers (they are not ours to fix).
get_filename_component(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
target_include_directories(speed_reaper PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(speed_reaper SYSTEM PRIVATE
  ${REPO_ROOT}/vendor/reaper-sdk/sdk
  ${REPO_ROOT}/vendor/WDL/WDL
)

if(MINGW)
  # Self-contained .dll: no libstdc++/libgcc dependency on the target.
  target_link_options(speed_reaper PRIVATE -static-libstdc++ -static-libgcc)
endif()

if(UNIX AND NOT APPLE)
  # SWELL on Linux resolves gdk-3.0 via pkg-config at build time (header-only;
  # REAPER loads GDK into SWELL at runtime, so nothing is linked).
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GDK3 REQUIRED gdk-3.0)
  target_include_directories(speed_reaper SYSTEM PRIVATE ${GDK3_INCLUDE_DIRS})
endif()
