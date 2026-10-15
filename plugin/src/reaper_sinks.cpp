// ReaperPlayheadSink / ReaperActionSink over the REAPER host API
// (moved out of main.cpp; no logic change).
//
// REAPER host APIs come from reaper_api.h — the single API list
// (REAPERAPI_MINIMAL + one WANT line per function). This TU includes it
// without REAPERAPI_IMPLEMENT (main.cpp owns the globals), so a called
// function with no WANT line is a link error.
#include "reaper_sinks.h"

#include "reaper_api.h"
#include "trace.h"

void ReaperPlayheadSink::ScrubBySeconds(double dt) {
  const int playing = GetPlayState() & 1;
  const double pos = playing ? GetPlayPosition() : GetCursorPosition();
  const double p = (pos + dt) < 0.0 ? 0.0 : pos + dt;
  SetEditCurPos(p, false, playing != 0);
}

double ReaperPlayheadSink::PlayPosition() const { return GetPlayPosition(); }

void ReaperActionSink::RunCommand(int cmd) {
  Trace("cmd %d", cmd); // oracle greps
  Main_OnCommand(cmd, 0);
}

void ReaperActionSink::RunCommandByName(const std::string &name) {
  const int id = NamedCommandLookup(name.c_str());
  if (id <= 0) {
    Trace("bind unresolved %s", name.c_str());
    return;
  }
  RunCommand(id);
}

void ReaperActionSink::ScrollBySeconds(double dt) {
  ReaProject *proj = nullptr;
  proj = EnumProjects(-1, nullptr, 0); // idx −1 = current project
  double t0 = 0.0, t1 = 0.0;
  GetSet_ArrangeView2(proj, false, 0, 0, &t0, &t1);
  t0 += dt;
  t1 += dt;
  GetSet_ArrangeView2(proj, true, 0, 0, &t0, &t1);
  UpdateArrange();
  Trace("scroll dt=%.4f", dt); // oracle greps
}

void ReaperActionSink::ApplyShuttleRate(double rate) {
  const int playing = GetPlayState() & 1;
  if (rate > 0.0) {
    if (playing == 0) return;
    const double capped = rate < 4.0 ? rate : 4.0; // no std::min: SDK min macro
    CSurf_OnPlayRateChange(capped);
    if (capped != last_rate_)
      Trace("shuttle rate=%.3f read=%.3f", capped, Master_GetPlayRate(nullptr));
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

void ReaperActionSink::HardStop() {
  if (GetPlayState() != 0) OnStopButton();
  CSurf_OnPlayRateChange(1.0);
  last_rate_ = 1.0;
}

void ReaperActionSink::Message(const char *msg) {
  Trace("message %s", msg);
  ShowConsoleMsg(msg);
}
