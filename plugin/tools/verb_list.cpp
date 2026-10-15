// verb_list — the tables the bindings reference (docs/bindings.md) is
// generated from (42-release-prep §2). Linked against the pure sources
// (the PLUGIN_PURE_SOURCES list, like hid_probe): no REAPER, no device.
//
// Usage:
//   verb_list --list-verbs   every registered verb as a Markdown table:
//                            name, accepted gestures, args, description
//                            (the verbs of MakeDefaultRegistry(), sorted)
//   verb_list --list-keys    every key of src/speed_editor/key_map.h as a
//                            Markdown table: binding name, printed label,
//                            keycode
//
// tools/gen_bindings_doc assembles docs/bindings.md from these two
// outputs plus the default bindings file verbatim; tools/check fails if
// the committed copy drifts from the registry.

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#include "bindings.h"        // AllKeys, KeyName
#include "speed_editor/key_map.h"  // getButtonNames (printed labels)
#include "verb_registry.h"   // MakeDefaultRegistry, VerbSpec, GestureName

namespace {

// The binding-facing gesture order (the kOrder in
// VerbRouter::DescribeBindings in verbs.cpp).
const BindingGesture kGestureOrder[] = {BindingGesture::Press,
                                        BindingGesture::Tap,
                                        BindingGesture::Double,
                                        BindingGesture::Turn};

// A table cell: '|' would break the Markdown row.
std::string Cell(const std::string &s) {
  std::string out;
  for (const char c : s) out += (c == '|') ? std::string("\\|") : std::string(1, c);
  return out;
}

// The argument syntax as the description writes it: every <...> and [...]
// token, in order of appearance ("<id>", "<move|...> [step=...]"); the
// em dash for verbs without arguments.
std::string ArgTokens(const std::string &desc) {
  std::vector<std::string> tokens;
  for (size_t i = 0; i < desc.size();) {
    const char open = desc[i];
    if (open != '<' && open != '[') {
      ++i;
      continue;
    }
    const char close = (open == '<') ? '>' : ']';
    const size_t j = desc.find(close, i + 1);
    if (j == std::string::npos) break;
    tokens.push_back(desc.substr(i, j - i + 1));
    i = j + 1;
  }
  if (tokens.empty()) return "\u2014";
  std::string out;
  for (const std::string &t : tokens) {
    if (!out.empty()) out += " ";
    out += t;
  }
  return out;
}

int ListVerbs() {
  const VerbRegistry reg = MakeDefaultRegistry();
  std::cout << "| verb | gestures | args | description |\n"
            << "| --- | --- | --- | --- |\n";
  for (const std::string &name : reg.Names()) {  // sorted
    const VerbSpec &spec = *reg.Find(name);
    std::string gestures;
    for (const BindingGesture ordered : kGestureOrder)
      for (const BindingGesture accepted : spec.gestures)
        if (accepted == ordered) {
          if (!gestures.empty()) gestures += ", ";
          gestures += GestureName(ordered);
        }
    std::cout << "| " << name << " | " << gestures << " | `"
              << Cell(ArgTokens(spec.description)) << "` | "
              << Cell(spec.description) << " |\n";
  }
  return 0;
}

int ListKeys() {
  const std::map<std::uint16_t, std::string> &labels = getButtonNames();
  std::cout << "| key | label | keycode |\n| --- | --- | --- |\n";
  for (const std::uint16_t key : AllKeys()) {  // key_map order
    const auto it = labels.find(key);
    char hex[8];
    std::snprintf(hex, sizeof hex, "0x%04X", key);
    std::cout << "| " << KeyName(key) << " | "
              << (it != labels.end() ? it->second : "\u2014") << " | "
              << hex << " |\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::fprintf(stderr,
                 "usage: verb_list --list-verbs | --list-keys\n");
    return 2;
  }
  if (std::string(argv[1]) == "--list-verbs") return ListVerbs();
  if (std::string(argv[1]) == "--list-keys") return ListKeys();
  std::fprintf(stderr, "verb_list: unknown option %s\n", argv[1]);
  return 2;
}
