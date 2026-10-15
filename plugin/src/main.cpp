// main.cpp — "Speed Reaper" REAPER extension entry point.
// Wires the Blackmagic Speed Editor (HID) to REAPER: the wheel feeds a
// JogEngine through mode/hold gating (jog → playhead, scroll → arrange
// view, shuttle → playrate), buttons drive transport and edit verbs via
// the router/gesture/verb layers, and a ~125 Hz timer ticks it all + LEDs.

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
#include "reaper_api.h"  // the single REAPER API list (MINIMAL + WANT_*, see that file)
#include "jog_engine.h"
#include "hid_wheel_source.h"
#include "action_sink.h"
#include "router.h" // Router, ModeGatedSource, kScrollUnitsPerStep (design §6)
#include "wheel_mode.h"
#include "gesture.h" // GestureEngine, Gesture, GestureKind (design §7)
#include "verbs.h" // VerbRouter, VerbDoubleTapKeys (design §7)
#include "led_state.h" // MainLedMask: SNAP LED follows REAPER's snap state
#include "reaper_project.h" // ReaperProject (M5 design §2; not wired into VerbRouter yet)
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
constexpr int kTimerMs = 8; // one engine step at 125 Hz
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
  // Shuttle rate (M4 decision, rule 2: docs/plugin-design-m4-shuttle-impl.md).
  // Forward: CSurf_OnPlayRateChange, clamped to REAPER's 4.0x read-back cap
  // (the 4…8x outer half of the ShuttleRate law is flat at 4.0x in transport
  // terms; no std::min here — the SDK min/max macros are in force). Reverse: synthesized — the playhead steps back |rate|·dt where dt
  // is one engine tick (kTimerMs = 8 ms; the Router calls this once per
  // tick). The Router never passes 0; 1.0 is the JOG/SCRL tap hand-off.
  // While the transport is stopped, forward/reverse park: the wheel still
  // drives the law and playback starts on a STOP/PLAY press (reverse while
  // stopped is scrub — JOG/SCRL's job).
  void ApplyShuttleRate(double rate) override {
    const int playing = GetPlayState() & 1;
    if (rate > 0.0) {
      if (playing == 0) return;
      const double capped = rate < 4.0 ? rate : 4.0; // no std::min: SDK min macro
      CSurf_OnPlayRateChange(capped);
      if (capped != last_rate_) Trace("shuttle rate=%.3f read=%.3f", capped, Master_GetPlayRate(nullptr));
      last_rate_ = capped;
    } else if (rate < 0.0) {
      if (playing == 0) return;
      const double dt = 8.0 / 1000.0; // one engine tick (kTimerMs), in seconds
      const double pos = GetPlayPositionEx(0);
      const double p = pos + rate * dt; // rate < 0 → step back
      SetEditCurPos(p < 0.0 ? 0.0 : p, false, 1); // seekplay: hold playback
      if (rate != last_rate_) Trace("shuttle reverse=%.3f pos=%.4f", rate, p);
      last_rate_ = rate;
    } else {
      CSurf_OnPlayRateChange(1.0); // hand-off: normal rate
      last_rate_ = 1.0;
    }
  }
  // Hard stop: SHTL dead band and STOP/PLAY-while-shuttling. OnStopButton,
  // never Main_OnCommand(40045) (blocks the script/defer queue headless,
  // M4 task 3); rate back to 1.0 for the next start.
  void HardStop() override {
    if (GetPlayState() != 0) OnStopButton();
    CSurf_OnPlayRateChange(1.0);
    last_rate_ = 1.0;
  }
  // One-line user message (M6, AUDIO LEVEL read-out): REAPER console.
  void Message(const char *msg) override {
    Trace("message %s", msg);
    ShowConsoleMsg(msg);
  }

 private:
  double last_rate_ = 1.0; // trace dedup: the Router re-applies every tick
};

ReaperPlayheadSink g_reaper_sink;
HidWheelSource g_hid; // Speed Editor HID wheel (task 6; worker thread)
ReaperActionSink g_action_sink;
Router g_router(g_action_sink); // owns the wheel mode (Jog/Scrl) from HID buttons
ReaperProject g_project; // IProject over the REAPER API (M5 design §2)
GestureEngine g_gestures; // tap / double-tap / hold recognition (design §7)
VerbRouter g_verb_router(g_action_sink, g_project); // gestures → verbs (design §7; M5 composites)

// Design §7: while any key is held, the HID wheel delta goes to the gesture
// engine (hold + rotate) — not the jog/scroll path. Held BEFORE the
// ModeGatedSource so the delta never reaches the Router or the engine's
// playhead/scroll code while a hold is active.
class HoldGatedSource : public IWheelSource {
 public:
  explicit HoldGatedSource(IWheelSource &hid) : hid_(hid) {}
  void Tick(WheelSample *out) override {
    WheelSample s;
    hid_.Tick(&s);
    if (g_gestures.HoldActive()) {
      g_gestures.OnWheel(s.delta, 0);  // now_ms is unused by OnWheel
      s.delta = 0;  // held key owns the wheel
    }
    *out = s;
  }

 private:
  IWheelSource &hid_;
};
HoldGatedSource g_hold_gated_source(g_hid); // design §7 hold+rotate wheel
ModeGatedSource g_gated_source(g_hold_gated_source, g_router); // HID delta: pass-through in Jog, scroll in Scrl
bool g_was_connected = false; // false→true edge re-sends mode + LEDs (design §6)
int g_led_mask = -1; // last pushed main-panel LED mask; -1 = not sent (force send)

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
  if (connected && !g_was_connected) {
    ApplyModeToHid(g_router.Mode());
    g_led_mask = -1; // re-send the main-panel LEDs after a reconnect edge
  }
  g_was_connected = connected;
  // LED bar (M8, design §5): poll the REAPER state (snap/ripple/trim-behind/
  // shared selected take/isolate/solo) every ~250 ms (~31 timer ticks at
  // REAPER's ~30 Hz) and push the main-panel mask only when it changes or
  // after a reconnect edge.
  static int led_tick = 0;
  if (g_led_mask < 0 || (led_tick % 31) == 0) {
    const std::uint32_t mask = MainLedMask(g_project.ReadLedState());
    if (static_cast<std::uint32_t>(g_led_mask) != mask) {
      g_led_mask = static_cast<int>(mask);
      Trace("led main=%x", mask); // oracle greps (M3 edit verbs task 5)
      g_hid.RequestLeds(mask);
    }
  }
  ++led_tick;
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
    // SHTL law (M4): absolute offset → ShuttleRate → sink. A no-op outside
    // Shtl mode; the sink only acts while the transport is playing.
    g_router.OnShuttleOffset(g_hid.AbsolutePosition());
    acc_ms -= kTimerMs;
    consumed_ms += kTimerMs;
  }
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

  g_engine = new JogEngine(g_gated_source, g_reaper_sink);
  g_sink = &g_reaper_sink;
  g_gestures.SetDoubleTapKeys(VerbDoubleTapKeys()); // IN, OUT, ESC (design §7)

  g_register("timer", (void *)&onTimer); // SDK: timer takes only void(*)()
  Trace("register timer ok");
  g_hid.Start(); // all registrations above succeeded; spawn the HID worker
  Trace("hid source started");
  return done(1);
}
