#include "verbs.h"

#include <cstdio>

#include "default_bindings_generated.h"  // kDefaultBindingsIni
#include "trace.h"

std::string_view DefaultBindingsText() { return kDefaultBindingsIni; }

VerbRouter::VerbRouter(IActionSink &sink, IProject &project,
                       std::string_view bindings_text)
    : sink_(sink), project_(project), registry_(MakeDefaultRegistry()) {
  Load(bindings_text);
}

void VerbRouter::Load(std::string_view text) {
  bindings_ = ParseBindings(text.empty() ? DefaultBindingsText() : text,
                            registry_);
  press_keys_.clear();
  for (const auto &kv : bindings_.bindings)
    if (kv.first.second == BindingGesture::Press)
      press_keys_.push_back(kv.first.first);
}

void VerbRouter::Reload(std::string_view text) { Load(text); }

void VerbRouter::EchoFire(std::uint16_t key, BindingGesture g,
                          const BoundVerb &b) {
  std::string line = KeyName(key) + "." + GestureName(g) + " \u2192 " + b.name;
  for (const std::string &a : b.args) line += " " + a;
  Trace("echo %s", line.c_str());
  if (echo_) sink_.Message((line + "\n").c_str());
}

std::vector<std::string> VerbRouter::DescribeBindings() const {
  static const BindingGesture kOrder[] = {BindingGesture::Press,
                                          BindingGesture::Tap,
                                          BindingGesture::Double,
                                          BindingGesture::Turn};
  std::vector<std::string> lines;
  std::string unbound;
  for (const std::uint16_t key : AllKeys()) {
    bool any = false;
    for (const BindingGesture g : kOrder) {
      const auto it = bindings_.bindings.find({key, g});
      if (it == bindings_.bindings.end()) continue;
      any = true;
      std::string verb = it->second.name;
      for (const std::string &a : it->second.args) verb += " " + a;
      const VerbSpec *spec = registry_.Find(it->second.name);
      char buf[256];
      std::snprintf(buf, sizeof buf, "%-12s %-6s %-24s %s\n",
                    KeyName(key).c_str(), GestureName(g), verb.c_str(),
                    spec ? spec->description.c_str() : "");
      lines.push_back(buf);
    }
    if (!any) unbound += (unbound.empty() ? "" : ", ") + KeyName(key);
  }
  lines.push_back("Unbound: " + unbound + "\n");
  return lines;
}

BoundVerb *VerbRouter::Find(std::uint16_t key, BindingGesture g) {
  const auto it = bindings_.bindings.find({key, g});
  return it == bindings_.bindings.end() ? nullptr : &it->second;
}

void VerbRouter::OnGesture(const Gesture &g) {
  VerbContext ctx{sink_, project_, bindings_.tuning.hold_ticks_per_step,
                  bindings_.tuning.wheel_pixels_per_detent};
  BindingGesture bg = BindingGesture::Tap;
  switch (g.kind) {
    case GestureKind::Press: bg = BindingGesture::Press; break;
    case GestureKind::Tap: bg = BindingGesture::Tap; break;
    case GestureKind::DoubleTap: bg = BindingGesture::Double; break;
    case GestureKind::HoldStart:
    case GestureKind::HoldRotate:
    case GestureKind::HoldEnd: bg = BindingGesture::Turn; break;
  }
  BoundVerb *b = Find(g.key, bg);
  if (!b) return;
  switch (g.kind) {
    case GestureKind::HoldStart:
      EchoFire(g.key, bg, *b);
      b->verb->Begin(ctx);
      return;
    case GestureKind::HoldRotate: b->verb->Step(ctx, g.delta); return;
    case GestureKind::HoldEnd: b->verb->End(ctx); return;
    default:
      EchoFire(g.key, bg, *b);
      b->verb->Run(ctx);
      return;
  }
}

bool VerbRouter::IsolateActive() const {
  for (const auto &kv : bindings_.bindings)
    if (kv.second.verb->IsolateActive()) return true;
  return false;
}

bool VerbRouter::SoloActive() const {
  for (const auto &kv : bindings_.bindings)
    if (kv.second.verb->SoloActive()) return true;
  return false;
}
