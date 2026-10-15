// user_bindings_test.cpp — the user bindings file path (42-release-prep §5):
// LoadUserBindings with a missing file (falls back to the embedded default
// and runs the seed callback once), an existing empty file (returns "" — the
// user cleared it, no fallback, no seed; parses to zero bindings, no crash),
// and a malformed file (returned verbatim; ParseBindings skips the bad lines
// with one error each and keeps the good ones). None of these may crash.
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

#include "bindings.h"

#include "check.h"
#include "fake_project.h"
#include "speed_editor/key_map.h"
#include "verbs.h"

namespace {

int seed_calls = 0;
const std::string kDefault = [] {
  return std::string(DefaultBindingsText());
}();

// mkstemp'd path (created, empty); returns it, or "" on failure.
std::string make_temp() {
  char path[] = "/tmp/speed_reaper_user_bindings_XXXXXX";
  int fd = ::mkstemp(path);
  if (fd < 0) return "";
  ::close(fd);
  return path;
}

void test_missing() {
  const std::string path = "/tmp/speed_reaper_user_bindings_missing_dir_x/bindings.ini";
  seed_calls = 0;
  std::string text = LoadUserBindings(path, kDefault, [&] { ++seed_calls; });
  CHECK(text == kDefault);  // falls back to the embedded default
  CHECK(seed_calls == 1);   // the plugin seeds the missing file exactly once
}

void test_empty() {
  const std::string path = make_temp();
  CHECK(!path.empty());
  if (path.empty()) return;
  seed_calls = 0;
  std::string text = LoadUserBindings(path, kDefault, [&] { ++seed_calls; });
  CHECK(text.empty());  // an empty file is "" (user cleared it), not the default
  CHECK(seed_calls == 0);
  const Bindings b = ParseBindings(text, MakeDefaultRegistry());
  CHECK(b.bindings.empty());
  CHECK(b.errors.empty());
  std::remove(path.c_str());
}

void test_malformed() {
  const std::string path = make_temp();
  CHECK(!path.empty());
  if (path.empty()) return;
  const std::string content =
      "[bindings]\n"
      "SPLIT.tap = action 40012\n"
      "FOOBAR.tap = action 40012\n"        // unknown key
      "SPLIT.frobnicate = action 40012\n"  // unknown gesture
      "TRIM_IN.tap = frobnicate\n";        // unknown verb
  std::ofstream out(path, std::ios::binary);
  out << content;
  out.close();
  seed_calls = 0;
  std::string text = LoadUserBindings(path, kDefault, [&] { ++seed_calls; });
  CHECK(text == content);  // the user file is read verbatim, bad lines and all
  CHECK(seed_calls == 0);
  const Bindings b = ParseBindings(text, MakeDefaultRegistry());
  CHECK(b.errors.size() == 3);  // one error per bad line, parsing did not stop
  CHECK(b.bindings.size() == 1);  // the good line survived
  const auto it = b.bindings.find({KEY_SPLIT, BindingGesture::Tap});
  CHECK(it != b.bindings.end());
  if (it != b.bindings.end()) CHECK(it->second.verb != nullptr);
  std::remove(path.c_str());
}

}  // namespace

int main() {
  test_missing();
  test_empty();
  test_malformed();
  return check_failures();
}
