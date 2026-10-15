// main.cpp — "Speed Reaper" REAPER extension entry point.
// Wires the Blackmagic Speed Editor (HID) to REAPER: all timer work lives
// in the pure Session (session.h); the REAPER-side code here is only the
// entry point, the timer callback (now_ms → Session::Tick → log drain),
// and the REAPER-backed sinks/project.

// Before SDK/Swell headers: their min/max macros break <limits>/<string>
// (hid_wheel_source.h needs <string> & friends; button_diff.h needs
// <vector>; include them here so they parse before the macros land).
#include <chrono>
#include <fstream>
#include <sstream>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "session.h" // pure Session (before the SDK: its headers define min/max)
#include "reaper_plugin.h"
#define REAPERAPI_IMPLEMENT // exactly one TU provides the REAPERAPI_* globals
#include "reaper_api.h"  // the single REAPER API list (MINIMAL + WANT_*, see that file)
#include "hid_wheel_source.h"
#include "reaper_project.h" // ReaperProject
#include "reaper_sinks.h"  // ReaperPlayheadSink / ReaperActionSink
#include "trace.h"

#include <cstdlib>
#include <cstring>

namespace {

// The SDK declares reaper_plugin_info_t::Register with two parameters, but
// REAPER's actual plugin_register is variadic ("timer" needs
// name, id, interval_ms, fn). Cast to the real prototype — same ABI, both
// cdecl (Linux) and thisvar/stdcall variadics (MSVC).
typedef int (*plugin_register_variadic)(const char *name, ...);
plugin_register_variadic g_register = nullptr;

IPlayheadSink *g_sink = nullptr;
Session *g_session = nullptr;

ReaperPlayheadSink g_reaper_sink;
HidWheelSource g_hid; // Speed Editor HID wheel (worker thread)
ReaperActionSink g_action_sink;
ReaperProject g_project; // IProject over the REAPER API

// <resource>/speed-reaper/bindings.ini: read it; if missing, write the
// embedded default (first run; never overwritten afterwards) and use that.
// Unreadable → empty text = the embedded default.
std::string LoadBindingsText() {
  const std::string dir = std::string(GetResourcePath()) + "/speed-reaper";
  const std::string path = dir + "/bindings.ini";
  {
    std::ifstream in(path, std::ios::binary);
    if (in) {
      std::ostringstream ss;
      ss << in.rdbuf();
      return ss.str();
    }
  }
  RecursiveCreateDirectory(dir.c_str(), 0);
  std::ofstream out(path, std::ios::binary);
  const std::string_view def = DefaultBindingsText();
  if (out) out.write(def.data(), static_cast<std::streamsize>(def.size()));
  Trace("bindings written %s", path.c_str());
  return std::string(def);
}

// --- callbacks driven by REAPER ---------------------------------------

void onTimer() {
  const std::int64_t now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  if (!g_session) return;
  g_session->Tick(now_ms);
  // Drain the HID worker's log lines and show them in REAPER's console.
  std::string line;
  while (g_hid.PopLog(&line))
    ShowConsoleMsg(("Speed Reaper: " + line + "\n").c_str());
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
    REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec) {
  (void)hInstance;

  // Trace everything the oracle scenario cannot see through REAPER's API.
  // Every return goes through `done`.
  auto done = [](int result) -> int {
    Trace("entry result=%d", result);
    return result;
  };

  if (!rec) { // REAPER unloading us
    Trace("unload");
    g_hid.Stop(); // join the worker before any pointers are cleared
    g_session = nullptr;
    g_sink = nullptr;
    g_register = nullptr;
    return 1;
  }

  Trace("entry rec=non-null");
  // Oracle test hook: prove crash capture.
  if (const char *crash = std::getenv("SPEED_REAPER_TEST_CRASH")) {
    if (std::strcmp(crash, "entry") == 0) std::abort();
  }

  int missing = REAPERAPI_LoadAPI(rec->GetFunc);
  Trace("loadapi missing=%d", missing);
  if (missing != 0) return done(0);
  g_register = (plugin_register_variadic)rec->Register;

  rec->Register("ext_name", (void *)"Speed Reaper");
  Trace("register ext_name ok");
  rec->Register("ext_url", (void *)"https://github.com/vkhougaz/speed-reaper");
  Trace("register ext_url ok");
  rec->Register("ext_vendor", (void *)"speed-reaper");
  Trace("register ext_vendor ok");

  g_session = new Session(g_hid, g_action_sink, g_project, g_reaper_sink,
                          LoadBindingsText());
  g_sink = &g_reaper_sink;

  g_register("timer", (void *)&onTimer); // SDK: timer takes only void(*)()
  Trace("register timer ok");
  g_hid.Start(); // all registrations above succeeded; spawn the HID worker
  Trace("hid source started");
  return done(1);
}
