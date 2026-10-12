// main.cpp — "Speed Reaper" hello-world REAPER extension.
//
// Demonstrates smooth playhead control through the exact pipeline the
// Blackmagic Speed Editor will use:
//
//   IWheelSource (today: DemoWheelSource; later: the hidapi driver from
//   src/speed_editor/ behind the same interface)
//        │  relative wheel deltas
//        ▼
//   JogEngine (1:1 delta → seconds)
//        │  seconds of timeline
//        ▼
//   IPlayheadSink → CSurf_ScrubAmt (REAPER scrub primitive)
//
// Actions (main section, search "Speed Reaper" in the action list):
//   "Speed Reaper: jog demo (smooth playhead)" — toggles a simulated wheel
//     turn; the playhead glides ~0.5 s forward and coasts to a stop.
//   "Speed Reaper: scrub forward 0.5 s" / "scrub back 0.5 s" — one-shot
//     nudge, verifies the sink path with no source involved.

// Before SDK/Swell headers: their min/max macros break <limits>/<string>
// (hid_wheel_source.h needs <string> & friends; button_diff.h needs
// <vector>; include them here so they parse before the macros land).
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
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
#define REAPERAPI_WANT_ShowConsoleMsg // onTimer: HID worker log lines
#define REAPERAPI_WANT_Main_OnCommand // ReaperActionSink::RunCommand (Router STOP/PLAY)
#define REAPERAPI_WANT_EnumProjects // ReaperActionSink::ScrollBySeconds (current project)
#define REAPERAPI_WANT_GetSet_ArrangeView2 // ReaperActionSink::ScrollBySeconds (read/shift view)
#define REAPERAPI_WANT_UpdateArrange // ReaperActionSink::ScrollBySeconds (redraw)
#include "reaper_plugin_functions.h"
#include "jog_engine.h"
#include "hid_wheel_source.h"
#include "action_sink.h"
#include "router.h" // Router, ModeGatedSource, kScrollUnitsPerStep (design §6)
#include "wheel_mode.h"
#include "gesture.h" // GestureEngine, Gesture, GestureKind (design §7)
#include "verbs.h" // VerbRouter, VerbDoubleTapKeys (design §7)
#include "trace.h"
#include "speed_editor/key_map.h" // KEY_* codes for the router-key split

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
// Max catch-up per timer callback. REAPER can delay timer delivery by >200 ms
// (observed 234 ms in the oracle) while its main loop and the oracle's
// sampling keep running; flushing the whole acc_ms backlog in one callback
// would move the cursor by up to ~0.07 s inside a single ~33 ms sample
// interval, which the oracle reads as stutter. Consume at most this much per
// callback and leave the rest in acc_ms for the next one. 50 ms covers the
// normal ~33 ms cycle (no steady-state drift) yet bounds the per-sample step.
constexpr double kMaxCatchUpMs = 50.0;

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
    const int playing = GetPlayState() & 1;
    const double pos = playing ? GetPlayPosition() : GetCursorPosition();
    const double p = (pos + dt) < 0.0 ? 0.0 : pos + dt;
    SetEditCurPos(p, false, playing != 0);
  }
  double PlayPosition() const override { return GetPlayPosition(); }
};

// --- IActionSink over the REAPER API (design §6) ---------------------

class ReaperActionSink : public IActionSink {
 public:
  // STOP/PLAY (40044) and friends: run a REAPER command.
  void RunCommand(int cmd) override {
    Trace("cmd %d", cmd); // oracle greps (docs/plugin-oracle-design.md §2)
    Main_OnCommand(cmd, 0);
  }
  // Scroll the arrange view by dt seconds of timeline (+ = right/later).
  // Same scale as the jog cursor (Router passes delta ×
  // JogEngine::kSecondsPerWheelUnit): the view travels the same on-screen
  // distance per wheel unit as the cursor, at any zoom level (seconds,
  // not screen lines — CSurf_OnScroll's line steps do not track zoom).
  void ScrollBySeconds(double dt) override {
    ReaProject *proj = nullptr;
    proj = EnumProjects(-1, nullptr, 0); // idx −1 = current project
    double t0 = 0.0, t1 = 0.0;
    GetSet_ArrangeView2(proj, false, 0, 0, &t0, &t1);
    t0 += dt;
    t1 += dt;
    GetSet_ArrangeView2(proj, true, 0, 0, &t0, &t1);
    UpdateArrange();
    Trace("scroll dt=%.4f", dt); // oracle greps (design §2)
  }
};

ReaperPlayheadSink g_reaper_sink;
DemoWheelSource g_demo_source; // synthetic wheel; demos the pipeline with no device
HidWheelSource g_hid; // Speed Editor HID wheel (task 6; worker thread)
ReaperActionSink g_action_sink;
Router g_router(g_action_sink); // owns the wheel mode (Jog/Scrl) from HID buttons
VerbRouter g_verb_router(g_action_sink); // recognized gestures → REAPER commands (design §7)
GestureEngine g_gestures; // tap / double-tap / hold recognition (design §7)
ModeGatedSource g_gated_source(g_hid, g_router); // HID delta: pass-through in Jog, scroll in Scrl
SummedWheelSource g_wheel_source(g_gated_source, g_demo_source);
bool g_was_connected = false; // false→true edge re-sends mode + LEDs (design §6)

const char *GestureKindName(GestureKind k) {
  switch (k) {
    case GestureKind::Tap: return "tap";
    case GestureKind::DoubleTap: return "double_tap";
    case GestureKind::HoldStart: return "hold_start";
    case GestureKind::HoldRotate: return "hold_rotate";
    case GestureKind::HoldEnd: return "hold_end";
  }
  return "?";
}

const char *WheelModeName(WheelMode m) {
  switch (m) {
    case WheelMode::Jog: return "Jog";
    case WheelMode::Scrl: return "Scrl";
    case WheelMode::Shtl: return "Shtl";
  }
  return "?";
}

// Forward the router's current mode to the device and light exactly the
// matching jog-mode LED (report 0x04: bit 0 JOG, 1 SHTL, 2 SCRL).
void ApplyModeToHid(WheelMode m) {
  Trace("mode %s", WheelModeName(m));
  g_hid.RequestWheelMode(ProtocolMode(m));
  const std::uint8_t jog_bits =
      (m == WheelMode::Jog ? 1u : m == WheelMode::Scrl ? 4u : 2u);
  Trace("led jog=%x main=%x", jog_bits, 0u); // no main-panel LEDs until M3
  g_hid.RequestJogLeds(m == WheelMode::Jog, m == WheelMode::Shtl,
                       m == WheelMode::Scrl);
}

// --- callbacks driven by REAPER ---------------------------------------

// REAPER's timer fires at ~30 Hz regardless of what we ask for, so run the
// engine in fixed kTimerMs steps to catch up with wall-clock time.
void onTimer() {
  // Drain the HID worker's log lines and show them in REAPER's console.
  std::string line;
  while (g_hid.PopLog(&line))
    ShowConsoleMsg(("Speed Reaper: " + line + "\n").c_str());
  // Buttons (design §6/§7): router keys (JOG/SCRL/SHTL/STOP-PLAY) go only
  // to the Router (press semantics live there); every other key goes only
  // to the gesture engine. A consumed mode change is forwarded to the
  // device + LEDs; a (re)connect re-sends current state.
  const std::int64_t now_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count();
  ButtonChange btn;
  while (g_hid.PopButton(&btn)) {
    switch (btn.key) {
      case KEY_JOG:
      case KEY_SCRL:
      case KEY_SHTL:
      case KEY_STOP_PLAY:
        g_router.OnButton(btn);
        break;
      default:
        g_gestures.OnKey(btn.key, btn.down, now_ms);
        break;
    }
  }
  g_gestures.Poll(now_ms); // every tick: advance tap-defer/hold timers
  Gesture gest;
  while (g_gestures.Pop(&gest)) {
    Trace("gesture %04x %s", gest.key, GestureKindName(gest.kind));
    g_verb_router.OnGesture(gest); // mapped → RunCommand (traces `cmd <id>`)
  }
  WheelMode changed;
  if (g_router.TakeModeChange(&changed)) ApplyModeToHid(changed);
  const bool connected = g_hid.Connected();
  if (connected && !g_was_connected) ApplyModeToHid(g_router.Mode());
  g_was_connected = connected;
  static auto last = std::chrono::steady_clock::now();
  static double acc_ms = 0.0;
  auto now = std::chrono::steady_clock::now();
  double gap_ms = std::chrono::duration<double, std::milli>(now - last).count();
  if (gap_ms > 100.0) Trace("timer gap=%.0f", gap_ms);  // REAPER UI stall
  acc_ms += gap_ms;
  last = now;
  if (acc_ms > 250.0) acc_ms = 250.0; // don't spiral after a stall
  static int calls = 0; // heartbeat so the oracle trace shows the timer is alive
  if ((++calls % 100) == 1) Trace("timer call=%d", calls);
  if (!g_engine) { acc_ms = 0.0; return; }
  double consumed_ms = 0.0;
  while (acc_ms >= kTimerMs && consumed_ms < kMaxCatchUpMs) {
    g_engine->Tick();
    acc_ms -= kTimerMs;
    consumed_ms += kTimerMs;
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
    g_hid.Stop(); // join the worker before any pointers are cleared
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

  rec->Register("ext_name", (void *)"Speed Reaper");
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

  g_engine = new JogEngine(g_wheel_source, g_reaper_sink);
  g_sink = &g_reaper_sink;
  g_gestures.SetDoubleTapKeys(VerbDoubleTapKeys()); // IN, OUT, ESC (design §7)

  // SDK header: "for actions, the related callback should be registered
  // with hookcommand2".
  rec->Register("hookcommand2", (void *)onHookCommand2);
  Trace("register hookcommand2 ok");
  g_register("timer", (void *)&onTimer); // SDK: timer takes only void(*)()
  Trace("register timer ok");
  g_hid.Start(); // all registrations above succeeded; spawn the HID worker
  Trace("hid source started");
  return done(1);
}
