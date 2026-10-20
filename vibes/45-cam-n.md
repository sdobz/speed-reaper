# Milestone 45 — CAM N: toggle item selection on track N (action-driven LEDs)

Multitrack comping (`docs/reaper-workflow.md` §8): track N plays the role of
"camera N". Tapping CAM N toggles the selection of the item under the cursor
on track N, so an editor can build a multi-track selection button by button.
The LED model changes with it: **a key's LED is owned by the action bound to
the key** — an association from verb (+args) to a lit/dark read, instead of a
hardcoded LED table. The old CAM behavior is removed entirely: the default
`CAM N.tap = take N` bindings go away, and the hardcoded "CAM LED = take
number shared by all selected items" is deleted (not kept as a fallback).
The `take` verb itself stays user-bindable (Hole H5) — it just has no default
and no LED.

> Facts that do not change: CAM key/LED ids in `src/speed_editor/key_map.h`
> (`KEY_CAM1..9`, `LED_CAM1..9`); the under-cursor containment rule in
> `plugin/src/target.h` (`pos <= cursor < pos+len`, cursor exactly at the end
> of the track's last item belongs to that item); the pure-layer pattern
> (`TimelineTap`/`CamSetTake` free functions in `plugin/src/verb_registry.cpp`,
> pure ops in `plugin/src/project_ops.h`). Oracle rulebook:
> `docs/notes/oracle-scenario-rules.md`. If a Hole in
> `docs/notes/cam-n-select.md` blocks a step, mark the step `- [!]` with a
> `BLOCKED:` line stating the measured values — do not guess.

## Context
- `docs/design.md` — §6 verbs, §9 targeting, §10 LED bar (this milestone rewrites §6's CAM row and §10)
- `docs/notes/cam-n-select.md` — created in step 1; **normative contract** for steps 2–4
- `docs/reaper-actions.tsv` — native action candidates (step 1)
- `plugin/src/led_state.h`, `plugin/src/project_ops.cpp` (`ReadLedState`), `plugin/src/verbs.h` (`VerbRouter`), `plugin/test/default_bindings_test.cpp`

### 1. Research: native candidates + selection behavior → `docs/notes/cam-n-select.md`

- `rg -i 'select' docs/reaper-actions.tsv` (Item section): list candidate "select/toggle item" actions with id + name; confirm existence of each via `NamedCommandLookup` in the probe.
- Add `oracle/scenarios/cam_probe.lua` (no fake device; model: `led_reads.lua`, ad-hoc — do NOT add to `REAPER_SCENARIOS`): two tracks, one item per track overlapping the edit cursor. Measure: (a) `B_UISEL` set/clear round-trip on the track-2 item with the track-1 selection untouched; (b) the edit cursor does not move when selection changes; (c) for each candidate action from the grep: run it in a known selection state and record the effect (toggle vs exclusive, which track/item it acts on, whether it is a no-op headless). Record every measurement as `check` details.
- Write `docs/notes/cam-n-select.md` — the contract (normative for steps 2–4):
  - Verb `select_track <N>` (1..9, tap only): toggles the selection bit of the item under the cursor on track N (1-based; containment rule from `target.h`); the rest of the selection is untouched (additive — the point is multi-track selection); no item under the cursor on that track → no-op + `Trace("target none")`; no undo block (like `TimelineTap`).
  - LED association: a key's LED is driven by the verb bound to its **tap** via `KeyLedLit(verb, args, cam, IProject&)` (pure, in `project_ops.h`). For now only `select_track` declares an LED: lit iff the item under the cursor on track `<arg>` is selected. Everything else (`take`, `action`, `targeted`, unbound) → dark. The LED follows the binding's argument (CAM1 bound to `select_track 3` lights on track-3 state; two keys bound to one track both light).
  - Shapes: `LedState.active_take` (int) is removed → `std::array<bool,10> cam` (index 1..9); `MainLedMask` ORs the lit `LED_CAM*` bits; `ReadLedState(IProject&, bool isolate, bool solo, const std::array<TapRef,10>& cam_taps)` where `TapRef { std::string verb; std::vector<std::string> args; }` (empty verb = unbound); new `VerbRouter::TapBinding(key, &verb, &args)`.
  - Scenario fallout: `leds_cam` (asserts the removed take-LED) is replaced by `cam_select` (step 4); `simple_verbs`'s CAM-2-take step is rebound to an explicit scenario-local binding (keep `take` e2e coverage as a *user* binding); `buttons` is unaffected (no items → both verbs no-op).
  - **Holes** (state each as open or resolved by your measurements; an unresolved hole that changes a contract in this doc must be surfaced with `- [!]` before step 2):
    - H1: does a usable native "toggle item selection under cursor" action exist (per-track or not)? If a per-track-capable native exists, say so — the custom verb is still expected (per-track arg is our requirement), but record the id as an alternative.
    - H2: `action <id>` / `targeted <id>` bindings (raw REAPER actions) cannot participate in the LED association (no args to read a track from). Accepted limitation — record it.
    - H3: only the **tap** binding drives the key's LED (press/double/turn do not). `select_track` accepts tap only, so this is consistent today.
    - H4: the cursor-exactly-at-item-end boundary for a *named* track cannot be measured headless unless a native candidate exists (H1). The unit test is the arbiter; record which.
    - H5: `take` verb stays in the registry (existing user `bindings.ini` files keep working; those keys just lose their LED until rebound). If you find a reason this breaks something, say so.
    - H6: users seeded with the old defaults keep working CAM buttons (take) but dark CAM LEDs — no migration (design §5: the user's file is theirs). Record as accepted.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario cam_probe && test -s docs/notes/cam-n-select.md && grep -q '^## Holes\|^- H[0-9]' docs/notes/cam-n-select.md
```

- [ ] Done

### 2. Verb `select_track` + new CAM defaults (unit tested)

- `plugin/src/target.h/.cpp`: `int CursorItemOnTrack(const std::vector<ItemInfo> &items, int track, double cursor)` — id or -1; the containment rule from `target.h` applied to one named track (refactor `CursorTargetItem` to use it if the rule overlaps).
- `plugin/src/verb_registry.cpp`: free function `void SelectTrackTap(IProject &project, int n)` (next to `TimelineTap`: find the item, flip its bit in the current selection set, `SetSelectedItems`; no target → `Trace("target none")`); register verb `select_track` — gestures `{Tap}`, arg `ParseIntArg`, range error `select_track: N must be 1..9`; class `SelectTrackVerb`.
- `plugin/src/default_bindings.ini`: `CAM1..9 .tap` → `select_track N` (replaces the nine `take N` lines; comments updated). Regenerate the reference: `tools/gen_bindings_doc docs/bindings.md` (tools/check fails if the committed copy is stale).
- `plugin/test/select_track_test.cpp`: toggle on → off → on; additive (an item selected on another track keeps its selection); no item under the cursor on that track → no-op; cursor exactly at the track's last-item end → that item (H4); arg 0 / 10 → parse error; `double`/`turn` gestures rejected. Update the CAM entries in `plugin/test/default_bindings_test.cpp` to `select_track`.
- Do not touch LED code yet (step 3); do not remove `take`/`CamSetTake`.

**Verify**
```bash
tools/check 'select_track|default_bindings'
```

- [ ] Done

### 3. Action-driven CAM LEDs (unit tested)

- Read `docs/notes/cam-n-select.md` § LED association first (normative shapes).
- `plugin/src/led_state.h/.cpp`: `LedState.active_take` → `std::array<bool,10> cam` (1..9); `MainLedMask` ORs the lit `LED_CAM*` bits (bit indexes from `key_map.h`).
- `plugin/src/project_ops.h/.cpp`: `TapRef` struct; `bool KeyLedLit(const std::string &verb, const std::vector<std::string> &args, int cam, IProject &p)` — the association table (only `select_track`: lit iff `CursorItemOnTrack(...)`'s item is selected; unknown verb or bad arg → false); `ReadLedState` gains the `cam_taps` parameter and drops the shared-take computation.
- `plugin/src/verbs.h/.cpp`: `bool VerbRouter::TapBinding(std::uint16_t key, std::string *verb, std::vector<std::string> *args) const`.
- `plugin/src/session.cpp` LED poll: build the `std::array<TapRef,10>` from `verbs_.TapBinding(KEY_CAM1..KEY_CAM9)` each poll (live — a reload is picked up automatically).
- Tests: `led_state_test.cpp` (two CAM bits OR together; no shared-take fallback exists anymore); `project_ops_test.cpp` (`KeyLedLit`: lit/unlit by selection, `take` → dark, unbound → dark, LED follows the binding's arg); `verbs_test.cpp` (`TapBinding` bound/unbound).

**Verify**
```bash
tools/check
```

- [ ] Done

### 4. Oracle: `cam_select` replaces `leds_cam` (reaper layer)

- First read `docs/notes/oracle-scenario-rules.md` (rebuild first, defer errors, two-attempt limit).
- Add `oracle/fake/cam_select.txt` + `oracle/scenarios/cam_select.env` + `cam_select.lua` (defaults — write no bindings file; two tracks, one item per track overlapping the cursor): select the track-2-under-cursor item (`B_UISEL`) → `led main=` gains CAM2 (bit 15, `0x8000`); fake taps CAM2 (`0x34` down/up) → the item deselects and the bit goes dark; tap again → dark → lit. Assert each `led main=` value, each within 1.5 s of its step (model: `leds_cam.lua`'s `within` usage).
- Delete `oracle/fake/leds_cam.txt`, `oracle/scenarios/leds_cam.lua`, `oracle/scenarios/leds_cam.env`; update `REAPER_SCENARIOS` in `tools/reaper-oracle` (`leds_cam` → `cam_select`).
- `simple_verbs`: the scenario now writes a `bindings.ini` with `CAM2.tap = take 2` before reload (model: `bindings.lua`) so the take step keeps e2e coverage as an explicit user binding under the new defaults; update its header comment.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario cam_select && tools/reaper-oracle --layer reaper --scenario simple_verbs
```

- [ ] Done

### 5. Docs: design §6/§10, README, SMOKE-TEST

- `docs/design.md`: §6 verb table — add `select_track | 1–9 | toggle the selection of the item under the cursor on track N`; §10 — replace the CAM row with the association model (a key's LED is owned by the action bound to its tap; `select_track` is the only declaration today; `take` and friends → dark).
- README "LED meanings" table: CAM1…CAM9 → "the item under the cursor on track N is selected (CAM N bound to `select_track`; any other binding → dark)".
- `SMOKE-TEST.md`: `## M45 — CAM N selection` — two-track setup; CAM N toggles the under-cursor item on track N (selection is additive across tracks); the CAM N LED follows the selection within ~1 s; tap again to reselect.

**Verify**
```bash
grep -q 'select_track' docs/design.md && grep -q '^## M45' SMOKE-TEST.md && grep -q 'select_track' SMOKE-TEST.md
```

- [ ] Done
