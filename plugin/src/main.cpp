// main.cpp — "Speed Reaper" hello-world REAPER extension.
//
// Demonstrates smooth playhead control through the exact pipeline the
// Blackmagic Speed Editor will use:
//
//   IWheelSource (today: DemoWheelSource; later: the hidapi driver from
//   src/speed_editor/ behind the same interface)
//        │  relative wheel deltas
//        ▼
//   JogEngine (exponential-velocity smoothing)
//        │  seconds of timeline
//        ▼
//   IPlayheadSink → CSurf_ScrubAmt (REAPER scrub primitive)
//
// Actions (main section, search "Speed Reaper" in the action list):
//   "Speed Reaper: jog demo (smooth playhead)" — toggles a simulated wheel
//     turn; the playhead glides ~0.5 s forward and coasts to a stop.
//   "Speed Reaper: scrub forward 0.5 s" / "scrub back 0.5 s" — one-shot
//     nudge, verifies the sink path with no source involved.

#include <chrono> // before SDK/Swell headers (their min/max macros break <limits>)
#include "reaper_plugin.h"
#define REAPERAPI_IMPLEMENT // exactly one TU provides the REAPERAPI_* globals
// REAPERAPI_MINIMAL: REAPERAPI_LoadAPI only probes the functions below
// instead of all ~877 SDK names. Without this, the entry refuses to load
// on any host whose GetFunc lacks even one SDK name (docs/notes/
// oracle-load-diagnosis.md).
// RULE: every REAPER API function a milestone adds to this plugin MUST get
// its REAPERAPI_WANT_<name> line in the same change, or LoadAPI probes it
// as missing and the plugin refuses to load.
#define REAPERAPI_MINIMAL
// --- REAPERAPI_WANT list: keep in sync with REAPER functions called here ---
#define REAPERAPI_WANT_GetPlayState // ReaperPlayheadSink::ScrubBySeconds
#define REAPERAPI_WANT_GetCursorPosition // ReaperPlayheadSink::ScrubBySeconds
#define REAPERAPI_WANT_SetEditCurPos // ReaperPlayheadSink::ScrubBySeconds
#define REAPERAPI_WANT_GetPlayPosition // ReaperPlayheadSink::PlayPosition
#include "reaper_plugin_functions.h"
#include "jog_engine.h"
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

// Engine step (ms): 125 Hz keeps the scrub motion smooth.
constexpr int kTimerMs = 8; // must match DemoWheelSource::kTickMs

IPlayheadSink *g_sink = nullptr;
JogEngine *g_engine = nullptr;
int g_cmd_demo = 0;
int g_cmd_fwd = 0;
int g_cmd_back = 0;

// --- IPlayheadSink over the REAPER API -------------------------------

class ReaperPlayheadSink : public IPlayheadSink {
 public:
  // Jog = move the edit cursor by dt seconds (docs/reaper-plugin-workflow.md
  // §3). Measured in the headless oracle: SetEditCurPos moves the cursor
  // exactly and linearly, while CSurf_ScrubAmt does not move it when the
  // transport is stopped (docs/notes/oracle-limits.md). While playing,
  // seek the playhead instead so playback follows the wheel.
  void ScrubBySeconds(double dt) override {
    if (GetPlayState() & 1) {
      double p = GetPlayPosition() + dt;
      SetEditCurPos(p < 0 ? 0 : p, false, true);
    } else {
      double p = GetCursorPosition() + dt;
      SetEditCurPos(p < 0 ? 0 : p, false, false);
    }
  }
  double PlayPosition() const override { return GetPlayPosition(); }
};

ReaperPlayheadSink g_reaper_sink;
DemoWheelSource g_demo_source; // synthetic wheel; swapped for the HID driver later

// --- callbacks driven by REAPER ---------------------------------------

// REAPER's timer fires at ~30 Hz regardless of what we ask for, so run the
// engine in fixed kTimerMs steps to catch up with wall-clock time.
void onTimer() {
  static auto last = std::chrono::steady_clock::now();
  static double acc_ms = 0.0;
  auto now = std::chrono::steady_clock::now();
  acc_ms += std::chrono::duration<double, std::milli>(now - last).count();
  last = now;
  if (acc_ms > 250.0) acc_ms = 250.0; // don't spiral after a stall
  static int calls = 0; // heartbeat so the oracle trace shows the timer is alive
  if ((++calls % 100) == 1) Trace("timer call=%d", calls);
  if (!g_engine) { acc_ms = 0.0; return; }
  while (acc_ms >= kTimerMs) {
    g_engine->Tick(kTimerMs);
    acc_ms -= kTimerMs;
  }
}

bool onHookCommand2(KbdSectionInfo *sec, int command, int val, int val2,
                    int relmode, HWND hwnd) {
  (void)sec; (void)val; (void)val2; (void)relmode; (void)hwnd;
  if (!g_engine) return false;
  if (command == g_cmd_demo) { // The engine ticks g_demo_source, so toggle that (not the engine's own
    // internal demo_ member, which nothing reads).
    if (g_demo_source.Active()) g_demo_source.Stop();
    else g_demo_source.Trigger();
    return true; }
  if (command == g_cmd_fwd) { g_engine->NudgeSeconds(+0.5); return true; }
  if (command == g_cmd_back) { g_engine->NudgeSeconds(-0.5); return true; }
  return false;
}

int RegisterAction(const char *id_str, const char *display_name) {
  custom_action_register_t ca = {0, id_str, display_name, nullptr};
  int cmd = g_register("custom_action", &ca); // command id (0 = failed)
  Trace("register action %s %s", id_str, cmd ? "ok" : "FAIL");
  if (cmd) Trace("action %s cmd=%d", id_str, cmd);
  return cmd;
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int ReaperPluginEntry(
    REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t *rec) {
  (void)hInstance;

  // Trace everything the oracle scenario cannot see through REAPER's API
  // (docs/plugin-oracle-design.md §2). Every return goes through `done`.
  auto done = [](int result) -> int {
    Trace("entry result=%d", result);
    return result;
  };

  if (!rec) { // REAPER unloading us
    Trace("unload");
    g_engine = nullptr;
    g_sink = nullptr;
    g_register = nullptr;
    return 1;
  }

  Trace("entry rec=non-null");
  // Oracle test hook (docs/plugin-oracle-design.md §2): prove crash capture.
  if (const char *crash = std::getenv("SPEED_REAPER_TEST_CRASH")) {
    if (std::strcmp(crash, "entry") == 0) std::abort();
  }

  int missing = REAPERAPI_LoadAPI(rec->GetFunc);
  Trace("loadapi missing=%d", missing);
  if (missing != 0) return done(0);
  g_register = (plugin_register_variadic)rec->Register;

  rec->Register("ext_name", (void *)"Speed Reaper (hello world)");
  Trace("register ext_name ok");
  rec->Register("ext_url", (void *)"https://github.com/vkhougaz/speed-reaper");
  Trace("register ext_url ok");
  rec->Register("ext_vendor", (void *)"speed-reaper");
  Trace("register ext_vendor ok");

  g_cmd_demo = RegisterAction("speed_reaper_jogdemo",
                              "Speed Reaper: jog demo (smooth playhead)");
  g_cmd_fwd = RegisterAction("speed_reaper_scrub_fwd",
                             "Speed Reaper: scrub forward 0.5 s");
  g_cmd_back = RegisterAction("speed_reaper_scrub_back",
                              "Speed Reaper: scrub back 0.5 s");
  if (!g_cmd_demo || !g_cmd_fwd || !g_cmd_back) return done(0);

  g_engine = new JogEngine(g_demo_source, g_reaper_sink);
  g_sink = &g_reaper_sink;

  // SDK header: "for actions, the related callback should be registered
  // with hookcommand2".
  rec->Register("hookcommand2", (void *)onHookCommand2);
  Trace("register hookcommand2 ok");
  g_register("timer", (void *)&onTimer); // SDK: timer takes only void(*)()
  Trace("register timer ok");
  return done(1);
}
