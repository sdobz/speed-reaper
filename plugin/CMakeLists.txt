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
# Windows: REAPER extensions must be built with MSVC (SDK header note; the
# C++ ABI must match REAPER's MSVC build — mingw is not supported), so the
# cross-toolchain in flake.nix cannot build this yet.

cmake_minimum_required(VERSION 3.13)
project(speed_reaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

if(WIN32 AND NOT MSVC)
  message(FATAL_ERROR
    "REAPER extensions for Windows require MSVC "
    "(see docs/reaper-dev-env.md §3). Build on Linux or use MSVC on Windows.")
endif()

add_library(speed_reaper SHARED
  src/main.cpp
  src/jog_engine.cpp
)

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

if(UNIX AND NOT APPLE)
  # SWELL on Linux resolves gdk-3.0 via pkg-config at build time (header-only;
  # REAPER loads GDK into SWELL at runtime, so nothing is linked).
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GDK3 REQUIRED gdk-3.0)
  target_include_directories(speed_reaper SYSTEM PRIVATE ${GDK3_INCLUDE_DIRS})
endif()
