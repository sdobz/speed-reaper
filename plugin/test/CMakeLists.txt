# Unit tests: every plugin/test/*_test.cpp becomes a ctest named <file>_test.
# Tests link the REAPER-free sources in PLUGIN_PURE_SOURCES (set by the
# parent CMakeLists.txt) — never reaper_plugin.h or the REAPER API.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*_test.cpp)
foreach(src ${TEST_SOURCES})
  get_filename_component(name ${src} NAME_WE)
  add_executable(${name} ${src} ${PLUGIN_PURE_SOURCES})
  target_include_directories(${name} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src)
  add_test(NAME ${name} COMMAND ${name})
endforeach()
