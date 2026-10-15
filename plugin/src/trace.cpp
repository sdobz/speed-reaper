// trace.cpp — implementation of Trace.
//
// Behavior: if env SPEED_REAPER_TRACE names a file, append
// "<ms since first trace> <message>\n" to it and fflush immediately (a
// crashed plugin must not lose trace lines); otherwise no-op. The env var
// is read on every call so tests/harnesses can point it at a temp file
// without restarting. Thread-safe: all state (start time, file) is guarded
// by a mutex; the per-call getenv is itself safe for concurrent readers
// as long as nobody mutates the environment concurrently (REAPER does not).

#include "trace.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {

std::mutex &Mutex() {
  static std::mutex m;
  return m;
}

std::chrono::steady_clock::time_point &Start() {
  static std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
  return t;
}

} // namespace

void Trace(const char *fmt, ...) {
  const char *path = std::getenv("SPEED_REAPER_TRACE");
  if (path == nullptr || path[0] == '\0') return;

  char msg[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);

  std::lock_guard<std::mutex> lock(Mutex());
  long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - Start())
                .count();
  std::FILE *f = std::fopen(path, "a");
  if (f == nullptr) return;
  std::fprintf(f, "%ld %s\n", ms, msg);
  std::fflush(f);
  std::fclose(f); // leaking this capped the trace at ~509 lines (CRT 512-stream limit)
}
