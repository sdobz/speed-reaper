#include "verbs.h"

#include "default_bindings_generated.h"  // kDefaultBindingsIni

std::string_view DefaultBindingsText() { return kDefaultBindingsIni; }

VerbRouter::VerbRouter(IActionSink &sink, IProject &project,
                       std::string_view bindings_text)
    : sink_(sink),
      project_(project),
      registry_(MakeDefaultRegistry()),
      bindings_(ParseBindings(
          bindings_text.empty() ? DefaultBindingsText() : bindings_text,
          registry_)) {
  for (const auto &kv : bindings_.bindings)
    if (kv.first.second == BindingGesture::Press)
      press_keys_.push_back(kv.first.first);
}

BoundVerb *VerbRouter::Find(std::uint16_t key, BindingGesture g) {
  const auto it = bindings_.bindings.find({key, g});
  return it == bindings_.bindings.end() ? nullptr : &it->second;
}

void VerbRouter::OnGesture(const Gesture &g) {
  VerbContext ctx{sink_, project_};
  BoundVerb *b = nullptr;
  switch (g.kind) {
    case GestureKind::Press:
      if ((b = Find(g.key, BindingGesture::Press))) b->verb->Run(ctx);
      return;
    case GestureKind::Tap:
      if ((b = Find(g.key, BindingGesture::Tap))) b->verb->Run(ctx);
      return;
    case GestureKind::DoubleTap:
      if ((b = Find(g.key, BindingGesture::Double))) b->verb->Run(ctx);
      return;
    case GestureKind::HoldStart:
      if ((b = Find(g.key, BindingGesture::Turn))) b->verb->Begin(ctx);
      return;
    case GestureKind::HoldRotate:
      if ((b = Find(g.key, BindingGesture::Turn)))
        b->verb->Step(ctx, g.delta);
      return;
    case GestureKind::HoldEnd:
      if ((b = Find(g.key, BindingGesture::Turn))) b->verb->End(ctx);
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
