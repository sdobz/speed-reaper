// trace.h — REAPER-free tracing for the Speed Reaper plugin
// (docs/plugin-oracle-design.md §2). Pure: no REAPER API, no GUI; linked
// into both the plugin and the unit tests.
//
// Trace("loadapi missing=%d", n); appends one line
// "<ms since first trace> loadapi missing=0\n" to the file named by env
// SPEED_REAPER_TRACE (and fflushes, so a crash loses nothing). If the env
// var is unset or empty, Trace is a no-op. Safe from any thread.
#ifndef SPEED_REAPER_TRACE_H
#define SPEED_REAPER_TRACE_H

void Trace(const char *fmt, ...);

#endif // SPEED_REAPER_TRACE_H
