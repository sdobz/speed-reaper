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
ReaperZoomSource g_reaper_zoom; // live arrange zoom (GetHZoomLevel; 46 §4.1)
HidWheelSource g_hid; // Speed Editor HID wheel (worker thread)
ReaperActionSink g_action_sink;
ReaperProject g_project; // IProject over the REAPER API

// <resource>/speed-reaper/bindings.ini: read it; if missing, write the
// embedded default (first run; never overwritten afterwards) and use that.
// Unreadable → empty text = the embedded default.
std::string LoadBindingsText() {
  const std::string dir = std::string(GetResourcePath()) + "/speed-reaper";
  const std::string path = dir + "/bindings.ini";
  const std::string_view def = DefaultBindingsText();
  return LoadUserBindings(path, def, [dir, path, def]() {
    RecursiveCreateDirectory(dir.c_str(), 0);
    std::ofstream out(path, std::ios::binary);
    if (out) out.write(def.data(), static_cast<std::streamsize>(def.size()));
    Trace("bindings written %s", path.c_str());
  });
}

// --- actions (action list: search "Speed Reaper") ----------------------

std::string BindingsPath() {
  return std::string(GetResourcePath()) + "/speed-reaper/bindings.ini";
}

void OpenBindingsFile() {
  LoadBindingsText();  // creates the file from the default if missing
  const std::string path = BindingsPath();
#if defined(_WIN32)
  const std::string cmd = "start \"\" \"" + path + "\"";
#elif defined(__APPLE__)
  const std::string cmd = "open '" + path + "' &";
#else
  const std::string cmd = "xdg-open '" + path + "' >/dev/null 2>&1 &";
#endif
  if (std::system(cmd.c_str()) != 0) Trace("open bindings failed");
}

struct SrAction {
  const char *id;    // command_id string (A-Za-z0-9 / _ ok for lookup)
  const char *desc;  // action list text
  bool toggle;
  int cmd;           // filled at registration
  gaccel_register_t accel;
};

SrAction g_actions[] = {
    {"SPEED_REAPER_RELOAD_BINDINGS", "Speed Reaper: reload bindings", false, 0, {}},
    {"SPEED_REAPER_OPEN_BINDINGS", "Speed Reaper: open bindings file", false, 0, {}},
    {"SPEED_REAPER_SHOW_BINDINGS", "Speed Reaper: show bindings", false, 0, {}},
    {"SPEED_REAPER_ECHO", "Speed Reaper: echo fired bindings (toggle)", true, 0, {}},
    {"SPEED_REAPER_LEARN", "Speed Reaper: learn (pick an action, then press a device key)", false, 0, {}},
};
constexpr int kNumActions = sizeof(g_actions) / sizeof(g_actions[0]);

// Learn: PromptForAction session (open, polled from the timer, closed).
bool g_prompt_open = false;

void StartLearnPrompt() {
  if (g_prompt_open) return;
  PromptForAction(1, 0, 0);  // open the action picker (main section)
  g_prompt_open = true;
  Trace("learn prompt open");
}

void PollLearnPrompt() {
  if (!g_prompt_open) return;
  const int id = PromptForAction(0, 0, 0);
  if (id == -1) { g_prompt_open = false; return; }  // window closed by the user
  if (id == 0) return;                              // nothing picked yet
  PromptForAction(-1, 0, 0);                        // got one: close the session
  g_prompt_open = false;
  const char *named = ReverseNamedCommandLookup(id);
  const std::string spec = named ? "_" + std::string(named) : std::to_string(id);
  if (g_session) g_session->BeginLearn(spec);
}

// The learned (KEY.gesture, "action <spec>"): write it into the file, reload.
void OnLearned(const std::string &lhs, const std::string &verb) {
  const std::string path = BindingsPath();
  const std::string text = SetBindingLine(LoadBindingsText(), lhs, verb);
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out.write(text.data(), static_cast<std::streamsize>(text.size()));
    else Trace("learn: cannot write %s", path.c_str());
  }
  if (g_session) g_session->ReloadBindings(text);
}

bool onCommand(int cmd, int /*flag*/) {
  if (cmd == 0 || !g_session) return false;
  for (int i = 0; i < kNumActions; ++i) {
    if (g_actions[i].cmd != cmd) continue;
    switch (i) {
      case 0: g_session->ReloadBindings(LoadBindingsText()); break;
      case 1: OpenBindingsFile(); break;
      case 2: g_session->ShowBindings(); break;
      case 3: g_session->ToggleEcho(); break;
      default: StartLearnPrompt(); break;
    }
    return true;
  }
  return false;
}

int onToggleState(int cmd) {
  for (const SrAction &a : g_actions)
    if (a.cmd == cmd && a.toggle) return g_session && g_session->EchoOn() ? 1 : 0;
  return -1;
}

void RegisterActions() {
  for (SrAction &a : g_actions) {
    a.cmd = g_register("command_id", a.id);
    if (a.cmd == 0) continue;
    a.accel.accel.cmd = static_cast<WORD>(a.cmd);
    a.accel.desc = a.desc;
    g_register("gaccel", &a.accel);
  }
  g_register("hookcommand", (void *)&onCommand);
  g_register("toggleaction", (void *)&onToggleState);
  Trace("register actions ok");
}

// --- callbacks driven by REAPER ---------------------------------------

void onTimer() {
  const std::int64_t now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  if (!g_session) return;
  PollLearnPrompt();
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
  // Version comes from plugin/CMakeLists.txt project(VERSION …);
  // "!SHOW:" appends to REAPER's console without popping the window.
  ShowConsoleMsg((std::string("!SHOW:Speed Reaper v") +
                  SPEED_REAPER_VERSION + " loaded\n").c_str());
  g_register = (plugin_register_variadic)rec->Register;

  rec->Register("ext_name", (void *)"Speed Reaper");
  Trace("register ext_name ok");
  rec->Register("ext_url", (void *)"https://github.com/vkhougaz/speed-reaper");
  Trace("register ext_url ok");
  rec->Register("ext_vendor", (void *)"speed-reaper");
  Trace("register ext_vendor ok");

  g_session = new Session(g_hid, g_action_sink, g_project, g_reaper_sink,
                          g_reaper_zoom, LoadBindingsText());
  g_session->SetLearnCallback(OnLearned);
  g_sink = &g_reaper_sink;

  RegisterActions();
  g_register("timer", (void *)&onTimer); // SDK: timer takes only void(*)()
  Trace("register timer ok");
  g_hid.Start(); // all registrations above succeeded; spawn the HID worker
  Trace("hid source started");
  return done(1);
}
