# Milestone 43 — LED refactor: one binding-coupled code path

Every main-panel LED bit is driven by one mechanism: a key's bit is lit iff
the state read of its highest-priority bound gesture holds. The hardcoded
`LedState`/`MainLedMask` path is deleted. Design is settled in
`docs/notes/led-binding.md`; this file only sequences the work. Milestone 45
(`select_track`, CAM rebinding) builds on it.

## Context
- `docs/notes/led-binding.md` — **normative** for every step (types, names, priority, defaults).
- `docs/notes/m8-state-reads.md` — measured toggle values for 1157/40310/40311/41117.
- `docs/design.md` §10 (LED bar), §6 (verbs).
- Tests live in `plugin/test/`; new test files must be added to `plugin/test/CMakeLists.txt`.

**Out of scope for the whole milestone:** jog-mode LEDs, the ~250 ms poll
interval, `select_track`, CAM rebinding, any state read beyond the four
seeded ones.

### 1. Named action ids
- Create `plugin/src/reaper_commands.h` per `docs/notes/led-binding.md` § Named ids.
- Replace `kSnapCmd`/`kTrimBehindCmd` in `plugin/src/project_ops.cpp` and `kRipplePerTrack`/`kRippleAll` in `plugin/src/reaper_project.cpp` with `reaper_cmd::*`.
- Do not change behavior; no new tests.

**Verify**
```bash
tools/check
```

- [x] Done

### 2. `SharedActiveTake`
- In `plugin/src/project_ops.{h,cpp}` add `int SharedActiveTake(IProject&)` (semantics in the design doc § Where reads live); `ReadLedState` calls it instead of its inline loop.
- Add unit tests in `plugin/test/project_ops_test.cpp`: shared take (1-based); 0 for nothing selected, mixed takes, take > 9.
- Do not delete `ReadLedState` yet.

**Verify**
```bash
tools/check project_ops
```

- [ ] Done

### 3. Provider plumbing on the registry (no providers yet)
- In `plugin/src/verb_registry.{h,cpp}` add `LedReadContext`, `StateRead`, `VerbSpec::state_read`, `VerbRegistry::StateReadFor` exactly as in the design doc § Types.
- Tests (`verb_registry_test`): a spec registered with a provider resolves through `StateReadFor`; a spec without one, and an unknown verb, give `nullopt`.
- Do not set `state_read` on any default verb yet.

**Verify**
```bash
tools/check verb_registry
```

- [ ] Done

### 4. Seed the four providers
- In `MakeDefaultRegistry` (`verb_registry.cpp`) set `state_read` on `action`, `take`, `isolate_tracks`, `solo_tracks` per the design doc § Initial providers; add `ActionStateReads()` there.
- Tests (`verb_registry_test`): `action 1157` true when snap on / false off; `action 40310`/`40311` follow `RippleMode()`; `action 41193` (not in table) → `nullopt`; named `action _x` → `nullopt`; `take 4` true iff `SharedActiveTake == 4`; `isolate_tracks`/`solo_tracks` follow the context; `ripple_delete` → `nullopt`.

**Verify**
```bash
tools/check verb_registry
```

- [ ] Done

### 5. `BindingLedMask`
- In `plugin/src/led_state.{h,cpp}` add `BindingLedMask` and the key→bit table per the design doc § Mask. Leave `LedState`/`MainLedMask` in place.
- Tests (`led_state_test`): tap beats double beats turn beats press; double-only key uses its double binding; `action 1157` lights the SNAP bit when snap is on; `action 41193` binding stays dark; `take N` lights the matching CAM bit; isolate/solo bits; unbound key dark.
- Do not edit `session.cpp`.

**Verify**
```bash
tools/check led_state
```

- [ ] Done

### 6. Realign default bindings
- `plugin/src/default_bindings.ini`: `CUT.tap = action 40310`, `DIS.tap = action 40311`, `LIVE_OWR.tap = action 41117` (comment each with the button↔LED coupling; drop the commented-out LIVE_OWR candidates that now conflict). Run `tools/gen_bindings_doc docs/bindings.md`.
- Update the three entries in `plugin/test/default_bindings_test.cpp`.
- `oracle/scenarios/xfade_keys.lua` (+ `oracle/fake/xfade_keys.txt` if the taps change): it asserts cmds `40916, 41059, 41193, 43776` via the defaults. Write a scenario-local `bindings.ini` binding the crossfade ids (40916, 41193) to keys so e2e coverage is kept (model: `oracle/scenarios/bindings.lua`).

**Verify**
```bash
tools/check default_bindings && tools/reaper-oracle --layer reaper --scenario xfade_keys
```

- [ ] Done

### 7. Wire the poll to `BindingLedMask`
- `plugin/src/verbs.{h,cpp}`: add `const Bindings &bindings() const` and `const VerbRegistry &registry() const`.
- `plugin/src/session.cpp` (~line 178): replace `MainLedMask(ReadLedState(...))` with `BindingLedMask(verbs_.bindings(), verbs_.registry(), LedReadContext{project_, verbs_.IsolateActive(), verbs_.SoloActive()})`.
- Do not delete the old functions yet; do not touch the jog-mode LED path.

**Verify**
```bash
tools/check && tools/reaper-oracle --layer reaper --scenario leds_ripple && tools/reaper-oracle --layer reaper --scenario leds_cam
```

- [ ] Done

### 8. Delete the hardcoded path
- Delete `LedState` (incl. `active_take`), `ledForCam` use, and `MainLedMask` from `led_state.{h,cpp}`; delete `ReadLedState` from `project_ops.{h,cpp}` (keep `SharedActiveTake`). Fix stale comments mentioning them (`reaper_api.h` line ~40, `led_state.h`).
- Migrate `plugin/test/led_read_test.cpp`, `led_state_test.cpp`, `session_test.cpp` (and `fake_project.h` comments) to `BindingLedMask`; keep their intent, drop nothing that still applies.

**Verify**
```bash
tools/check && ! rg -n 'MainLedMask|ReadLedState|active_take' plugin
```

- [ ] Done

### 9. Oracle proof: LED follows the binding
- Add `oracle/scenarios/leds_binding_driven.{lua,env}` + `oracle/fake/leds_binding_driven.txt`, and the name to `REAPER_SCENARIOS` in `tools/reaper-oracle` (model: `leds_ripple.*`). The scenario writes a `bindings.ini` rebinding an LED-bearing key (e.g. CLOSE_UP) to `action 40311` and asserts that key's bit tracks ripple-all state (and SNAP's own bit is unaffected).

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario leds_binding_driven
```

- [ ] Done

### 10. Docs
- `docs/design.md` §10: say each LED reflects the state read of the key's bound verb (priority Tap→Double→Turn→Press), list the four seeded reads + defaults; link `docs/notes/led-binding.md`. README "LED meanings": same, mention LEDs follow the *binding*.

**Verify**
```bash
tools/check
```

- [ ] Done
