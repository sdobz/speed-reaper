# LED bar: one binding-coupled mechanism (design for vibes/43)

Normative for `vibes/43-led-refactor.md` and `vibes/45-cam-n.md`.

## Rule

A main-panel key's LED bit is lit iff the **state read** of the key's
highest-priority bound gesture is present and true against live state. No
key identity, no hardcoded action list in the mask. (Jog-mode LEDs, report
0x04, are a separate path and untouched.)

## Types (all in `plugin/src/verb_registry.h`)

```cpp
struct LedReadContext { IProject &project; bool isolate_active = false; bool solo_active = false; };
using StateRead = std::function<bool(const LedReadContext &)>;
// VerbSpec gains (absent by default):
std::function<std::optional<StateRead>(const std::vector<std::string> &args)> state_read;
// VerbRegistry gains (NOT named StateRead: that is the type above):
std::optional<StateRead> StateReadFor(const std::string &verb, const std::vector<std::string> &args) const;
```

`StateReadFor` looks up the spec and runs its provider; no spec or no
provider → `nullopt`. The provider returns the read for that *specific*
binding or `nullopt` (one-shot action, named `action <name>`, …).
**Extension point:** giving any verb an LED state = set `state_read` in its
registration. No mask, core, or other-verb change.

## Mask (`plugin/src/led_state.{h,cpp}`)

`std::uint32_t BindingLedMask(const Bindings&, const VerbRegistry&, const LedReadContext&)`
— pure. For each of the 18 LED-bearing keys (key → bit table from
`speed_editor/key_map.h`, defined in `led_state.cpp`): take the first bound
gesture in the order **Tap → Double → Turn → Press**; call
`registry.StateReadFor(verb, args)`; set the bit iff present and true.
Unbound / no read → dark. `led_state.h` forward-declares `Bindings`,
`VerbRegistry`, `LedReadContext`.

## Initial providers (in `MakeDefaultRegistry`)

| verb | read |
|---|---|
| `action <id>` | `ParseCommandArg`; named id (`cmd.named`) or id not in `ActionStateReads()` → none; else that table entry |
| `take <N>` | `ParseIntArg`; true iff `SharedActiveTake(project) == N` |
| `isolate_tracks` | `ctx.isolate_active` |
| `solo_tracks` | `ctx.solo_active` |
| everything else | unset → dark |

`ActionStateReads()` = `std::map<int, StateRead>` in an anonymous namespace
in `verb_registry.cpp`, keyed by `reaper_cmd::*`: Snap and TrimBehind →
`project.ToggleState(id)`; RipplePerTrack → `project.RippleMode() == 1`;
RippleAll → `project.RippleMode() == 2`. Values proven in
`docs/notes/m8-state-reads.md`.

## Named ids — `plugin/src/reaper_commands.h`

```cpp
namespace reaper_cmd { constexpr int Snap = 1157; constexpr int RipplePerTrack = 40310;
                       constexpr int RippleAll = 40311; constexpr int TrimBehind = 41117; }
```
The raw numbers appear only here. Replaces `kSnapCmd`/`kTrimBehindCmd`
(project_ops.cpp) and `kRipplePerTrack`/`kRippleAll` (reaper_project.cpp).

## Where reads live

1. Project queries: `IProject::ToggleState`, `IProject::RippleMode` (exist);
   new pure `int SharedActiveTake(IProject&)` in `project_ops.{h,cpp}`,
   extracted from `ReadLedState` (1-based; 0 if nothing selected / no active
   take / mixed / take > 9).
2. Association (binding → read): the providers + `ActionStateReads()` in
   `verb_registry.cpp`.
3. Plugin state: isolate/solo carried in `LedReadContext`, built in
   `session.cpp` from `VerbRouter::IsolateActive()/SoloActive()`.

`VerbRouter` exposes `bindings()` and `registry()` const accessors (not
`Bindings()`: that name would hide the `Bindings` type).

## Default realignment (button action == LED state)

| key | old default | new default | old LED | new LED | steady-state LED same? |
|---|---|---|---|---|---|
| CUT | `action 41193` (remove fades) | `action 40310` | per-track ripple | read of `action 40310` | yes |
| DIS | `action 40916` (crossfade in time sel) | `action 40311` | ripple-all | read of `action 40311` | yes |
| LIVE_OWR | unbound | `action 41117` | trim-behind | read of `action 41117` | yes |
| SNAP | `action 1157` | unchanged | snap | `action 1157` read | yes |
| VIDEO_ONLY / AUDIO_ONLY | isolate / solo | unchanged | view state | provider | yes |
| CAM1–9 | `take N` | unchanged here (45 rebinds) | shared take | `take N` read | yes |
| SMTH_CUT, TRANS, CLOSE_UP | one-shot/unbound | unchanged | dark | dark | yes |

CUT/DIS button *actions* change (ripple toggles replace the crossfade
actions); LIVE_OWR gains a binding. Existing user `bindings.ini` files are
never rewritten (design §5).

## Tests/scenarios affected

`xfade_keys` (relies on default CUT/DIS ids 41193/40916 → bind them as
scenario-local user bindings), `default_bindings_test`, `led_state_test`,
`led_read_test`, `session_test` (uses `ReadLedState`/`MainLedMask`),
`verb_registry_test`; `leds_ripple` and `leds_cam` must keep passing.
