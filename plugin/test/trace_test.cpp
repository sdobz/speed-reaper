// Characterizes Trace (docs/plugin-oracle-design.md §2) without REAPER.
// SPEED_REAPER_TRACE set to a temp file: two Trace calls → two lines, each
// ending in the message. Unset: no file is created.
#include "check.h"
#include "trace.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string FileContent(const char *path) {
  std::string out;
  std::FILE *f = std::fopen(path, "rb");
  if (f) {
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
  }
  return out;
}

// "<ms> <msg>\n" — everything after the first space is the message.
bool LineEndsWith(const std::string &line, const char *msg) {
  return line.size() > std::strlen(msg) &&
         line.compare(line.size() - std::strlen(msg), std::strlen(msg), msg) == 0;
}

} // namespace

int main() {
  const char *saved = std::getenv("SPEED_REAPER_TRACE");
  const bool was_set = saved != nullptr;

  char path[] = "/tmp/speed_reaper_trace_test_XXXXXX";
  int fd = mkstemp(path);
  CHECK(fd >= 0);
  if (fd >= 0) {
    close(fd);
    std::remove(path); // start from a non-existent file
  }

  // Env set → appends one line per call, each ending in the message.
  if (fd >= 0) {
    setenv("SPEED_REAPER_TRACE", path, 1);
    Trace("hello %d", 42);
    Trace("second");
    std::string content = FileContent(path);
    int nlines = 0;
    for (char c : content) nlines += (c == '\n');
    CHECK(nlines == 2);
    std::string first = content.substr(0, content.find('\n'));
    std::string second = content.substr(content.find('\n') + 1);
    if (!second.empty() && second.back() == '\n') second.pop_back();
    CHECK(LineEndsWith(first, "hello 42"));
    CHECK(LineEndsWith(second, "second"));
    CHECK(first.find(' ') != std::string::npos); // "<ms> <message>" shape
    std::remove(path);
  }

  // Regression: a leaked FILE* per call capped the trace at ~509 lines
  // (Windows CRT 512-stream limit). Far more lines than that must all land.
  if (fd >= 0) {
    setenv("SPEED_REAPER_TRACE", path, 1);
    for (int i = 0; i < 2000; i++) Trace("line %d", i);
    std::string content = FileContent(path);
    int nlines = 0;
    for (char c : content) nlines += (c == '\n');
    CHECK(nlines == 2000);
    std::remove(path);
  }

  // Env unset → no file is created.
  unsetenv("SPEED_REAPER_TRACE");
  Trace("should not be written");
  struct stat st;
  CHECK(stat(path, &st) != 0 || errno == ENOENT);

  if (was_set) setenv("SPEED_REAPER_TRACE", saved, 1);
  return check_failures();
}
