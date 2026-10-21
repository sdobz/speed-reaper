// bindings.cpp — the bindings.ini parser (41 design §2, pure).
#include "bindings.h"

#include <cctype>
#include <climits>
#include <cstdlib>  // std::strtol
#include <fstream>
#include <sstream>
#include <utility>

#include "speed_editor/key_map.h"  // KEY_* keycodes

namespace {

const struct {
  const char *name;
  std::uint16_t code;
} kKeyNames[] = {
    {"SMART_INSRT", KEY_SMART_INSRT},
    {"APPND", KEY_APPND},
    {"RIPL_OWR", KEY_RIPL_OWR},
    {"CLOSE_UP", KEY_CLOSE_UP},
    {"PLACE_ON_TOP", KEY_PLACE_ON_TOP},
    {"SRC_OWR", KEY_SRC_OWR},
    {"IN", KEY_IN},
    {"OUT", KEY_OUT},
    {"TRIM_IN", KEY_TRIM_IN},
    {"TRIM_OUT", KEY_TRIM_OUT},
    {"ROLL", KEY_ROLL},
    {"SLIP_SRC", KEY_SLIP_SRC},
    {"SLIP_DEST", KEY_SLIP_DEST},
    {"TRANS_DUR", KEY_TRANS_DUR},
    {"CUT", KEY_CUT},
    {"DIS", KEY_DIS},
    {"SMTH_CUT", KEY_SMTH_CUT},
    {"SOURCE", KEY_SOURCE},
    {"TIMELINE", KEY_TIMELINE},
    {"SHTL", KEY_SHTL},
    {"JOG", KEY_JOG},
    {"SCRL", KEY_SCRL},
    {"SYNC_BIN", KEY_SYNC_BIN},
    {"TRANS", KEY_TRANS},
    {"VIDEO_ONLY", KEY_VIDEO_ONLY},
    {"AUDIO_ONLY", KEY_AUDIO_ONLY},
    {"RIPL_DEL", KEY_RIPL_DEL},
    {"AUDIO_LEVEL", KEY_AUDIO_LEVEL},
    {"FULL_VIEW", KEY_FULL_VIEW},
    {"SNAP", KEY_SNAP},
    {"SPLIT", KEY_SPLIT},
    {"LIVE_OWR", KEY_LIVE_OWR},
    {"ESC", KEY_ESC},
    {"CAM1", KEY_CAM1},
    {"CAM2", KEY_CAM2},
    {"CAM3", KEY_CAM3},
    {"CAM4", KEY_CAM4},
    {"CAM5", KEY_CAM5},
    {"CAM6", KEY_CAM6},
    {"CAM7", KEY_CAM7},
    {"CAM8", KEY_CAM8},
    {"CAM9", KEY_CAM9},
    {"STOP_PLAY", KEY_STOP_PLAY},
};

std::string_view Trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.remove_suffix(1);
  return s;
}

// The verb's name + args, whitespace-separated.
std::vector<std::string> Tokenize(std::string_view s) {
  std::vector<std::string> out;
  while (true) {
    s = Trim(s);
    if (s.empty()) break;
    std::size_t sp = s.find(' ');
    if (sp == std::string_view::npos) sp = s.find('\t');
    if (sp == std::string_view::npos) {
      out.push_back(std::string(s));
      break;
    }
    out.push_back(std::string(s.substr(0, sp)));
    s = s.substr(sp + 1);
  }
  return out;
}

// A full integer (the tuning values and the int-arg range rule).
bool ParseInt(const std::string &s, int *out) {
  if (s.empty()) return false;
  char *end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (end == s.c_str() || *end != '\0') return false;
  if (v < INT_MIN || v > INT_MAX) return false;
  *out = static_cast<int>(v);
  return true;
}

bool ParseGesture(const std::string &name, BindingGesture *out) {
  if (name == "press") {
    *out = BindingGesture::Press;
    return true;
  }
  if (name == "tap") {
    *out = BindingGesture::Tap;
    return true;
  }
  if (name == "double") {
    *out = BindingGesture::Double;
    return true;
  }
  if (name == "turn") {
    *out = BindingGesture::Turn;
    return true;
  }
  return false;
}

}  // namespace

std::uint16_t KeyByName(const std::string &name) {
  for (const auto &k : kKeyNames)
    if (name == k.name) return k.code;
  return 0;
}

std::string KeyName(std::uint16_t key) {
  for (const auto &k : kKeyNames)
    if (key == k.code) return k.name;
  return "";
}

std::vector<std::uint16_t> AllKeys() {
  std::vector<std::uint16_t> keys;
  for (const auto &k : kKeyNames) keys.push_back(k.code);
  return keys;
}

std::string SetBindingLine(std::string_view ini, std::string_view lhs,
                           std::string_view rhs) {
  std::vector<std::string> lines;
  for (size_t pos = 0; pos <= ini.size();) {
    size_t nl = ini.find('\n', pos);
    if (nl == std::string_view::npos) {
      if (pos < ini.size()) lines.emplace_back(ini.substr(pos));
      break;
    }
    lines.emplace_back(ini.substr(pos, nl - pos));
    pos = nl + 1;
  }
  const std::string newline = std::string(lhs) + " = " + std::string(rhs);
  bool in_bindings = false;
  int last_content = -1;  // last non-blank line inside [bindings]
  int bindings_header = -1;
  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string_view t = Trim(lines[i]);
    if (!t.empty() && t.front() == '[') {
      in_bindings = (t == "[bindings]");
      if (in_bindings) { bindings_header = static_cast<int>(i); last_content = static_cast<int>(i); }
      continue;
    }
    if (!in_bindings || t.empty()) continue;
    last_content = static_cast<int>(i);
    if (t.front() == ';' || t.front() == '#') continue;
    const size_t eq = t.find('=');
    if (eq != std::string_view::npos && Trim(t.substr(0, eq)) == lhs) {
      lines[i] = newline;
      goto done;
    }
  }
  if (bindings_header < 0) {
    if (!lines.empty() && !Trim(lines.back()).empty()) lines.push_back("");
    lines.push_back("[bindings]");
    lines.push_back(newline);
  } else {
    lines.insert(lines.begin() + last_content + 1, newline);
  }
done:
  std::string out;
  for (const std::string &l : lines) out += l + "\n";
  return out;
}

Bindings ParseBindings(std::string_view text, const VerbRegistry &registry) {
  Bindings out;
  enum class Section { None, Tuning, Bindings };
  Section section = Section::None;
  // press-exclusivity, per key: a `press` binding forbids every other
  // gesture on the key, and vice versa (checked as lines arrive, so the
  // offending line is the one reported and skipped).
  std::map<std::uint16_t, bool> key_has_press;
  std::map<std::uint16_t, bool> key_has_other;

  auto err = [&out](int line, const std::string &reason) {
    out.errors.push_back("bindings.ini:" + std::to_string(line) + ": " +
                         reason);
  };

  std::string_view rest = text;
  int line = 0;
  while (!rest.empty()) {
    const std::size_t nl = rest.find('\n');
    std::string_view raw =
        (nl == std::string_view::npos) ? rest : rest.substr(0, nl);
    rest = (nl == std::string_view::npos) ? "" : rest.substr(nl + 1);
    ++line;

    // `;` / `#` starts a comment anywhere (strip the earliest of the two).
    const std::size_t semi = raw.find(';');
    const std::size_t hash = raw.find('#');
    if (semi != std::string_view::npos &&
        (hash == std::string_view::npos || semi < hash))
      raw = raw.substr(0, semi);
    else if (hash != std::string_view::npos) raw = raw.substr(0, hash);
    raw = Trim(raw);
    if (raw.empty()) continue;

    if (raw.front() == '[' && raw.back() == ']') {
      const std::string name(std::string(Trim(raw.substr(1, raw.size() - 2))));
      if (name == "tuning")
        section = Section::Tuning;
      else if (name == "bindings")
        section = Section::Bindings;
      else
        err(line, "unknown section '" + name + "'");
      continue;
    }

    const std::size_t eq = raw.find('=');
    if (eq == std::string_view::npos) {
      err(line, "expected 'key = value'");
      continue;
    }
    const std::string lhs(Trim(raw.substr(0, eq)));
    const std::string rhs(Trim(raw.substr(eq + 1)));

    if (section == Section::None) {
      err(line, "line outside a section");
      continue;
    }

    if (section == Section::Tuning) {
      int value = 0;
      if (!ParseInt(rhs, &value)) {
        err(line, "tuning value '" + rhs + "' is not an integer");
        continue;
      }
      if (lhs == "double_tap_ms")
        out.tuning.double_tap_ms = value;
      else if (lhs == "hold_ms")
        out.tuning.hold_ms = value;
      else if (lhs == "hold_ticks_per_step")
        out.tuning.hold_ticks_per_step = value;
      else
        err(line, "unknown tuning key '" + lhs + "'");
      continue;
    }

    // [bindings]: KEY.gesture = verb [args]
    const std::size_t dot = lhs.find('.');
    if (dot == std::string::npos) {
      err(line, "expected KEY.gesture, got '" + lhs + "'");
      continue;
    }
    const std::string key_name(Trim(lhs.substr(0, dot)));
    const std::string gesture_name(Trim(lhs.substr(dot + 1)));
    const std::uint16_t key = KeyByName(key_name);
    if (key == 0) {
      err(line, "unknown key '" + key_name + "'");
      continue;
    }
    BindingGesture gesture;
    if (!ParseGesture(gesture_name, &gesture)) {
      err(line, "unknown gesture '" + gesture_name + "'");
      continue;
    }
    const std::vector<std::string> tokens = Tokenize(rhs);
    if (tokens.empty()) {
      err(line, "missing verb");
      continue;
    }
    if (gesture == BindingGesture::Press && key_has_other[key]) {
      err(line, "key '" + KeyName(key) +
                     "' is bound to 'press' and must not have other gestures");
      continue;
    }
    if (gesture != BindingGesture::Press && key_has_press[key]) {
      err(line, "key '" + KeyName(key) +
                     "' has a 'press' binding and must not have other gestures");
      continue;
    }
    const std::string verb_name = tokens[0];
    std::vector<std::string> args(tokens.begin() + 1, tokens.end());
    VerbBuildResult built = registry.Build(verb_name, gesture, args);
    if (!built.error.empty()) {
      err(line, built.error);
      continue;
    }
    if (gesture == BindingGesture::Press)
      key_has_press[key] = true;
    else
      key_has_other[key] = true;
    BoundVerb bound;
    bound.name = verb_name;
    bound.args = args;
    bound.verb = std::move(built.verb);
    out.bindings.insert_or_assign({key, gesture}, std::move(bound));
  }

  for (const auto &entry : out.bindings)  // map order: keys already sorted
    if (entry.first.second == BindingGesture::Double)
      out.double_tap_keys.push_back(entry.first.first);
    else if (entry.first.second == BindingGesture::Turn)
      out.turn_keys.push_back(entry.first.first);
  return out;
}

std::string LoadUserBindings(const std::string &path,
                             std::string_view fallback,
                             const std::function<void()> &seed_if_missing) {
  std::ifstream in(path, std::ios::binary);
  if (in) {
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();  // verbatim, empty file included: the user cleared it
  }
  if (seed_if_missing) seed_if_missing();
  return std::string(fallback);  // missing/unreadable: the embedded default
}
