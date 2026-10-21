# Milestone 45 — CAM N: toggle item selection on track N

Multitrack comping (`docs/reaper-workflow.md` §8): track N plays "camera N".
A tap on a key bound to `select_track N` toggles the selection of the item
under the edit cursor on track N (additive across tracks), and that key's LED
is lit iff that item is selected — state-driven, so it lights without the key
ever being pressed. **Requires milestone 43** (the `state_read` provider on
`VerbSpec`, `BindingLedMask`): 45 adds one verb with one provider and rebinds
the CAM defaults; it adds no LED mechanism.

## Context
- `docs/notes/led-binding.md` — the LED mechanism (from 43); **do not redesign it**.
- `docs/notes/cam-n-select.md` — the contract (settled decisions); step 1 appends measurements; **normative** for every step.
- `docs/design.md` §6 (verbs), §9 (targeting), §10 (LED bar); `docs/reaper-actions.tsv`.
- `plugin/src/target.{h,cpp}` — under-cursor rule: `pos <= cursor < pos+len`; a boundary between items belongs to the one starting there; cursor exactly at a track's last item end belongs to that item.
- `plugin/src/verb_registry.cpp` — `TimelineTap` is the model for a pure verb function.
- `docs/notes/oracle-scenario-rules.md` — read before any oracle step (rebuild first, defer errors, two-attempt limit).

### 1. Research: native candidates → `docs/notes/cam-n-select.md`
- `rg -i 'select' docs/reaper-actions.tsv`: list candidate native "select/toggle item" actions (id + name).
- Add `oracle/scenarios/cam_probe.lua` (ad-hoc, no fake device, NOT in `REAPER_SCENARIOS`; model: `oracle/scenarios/led_reads.lua`): two tracks, one item each overlapping the cursor. Record as `check` details: (a) `B_UISEL` set/clear on the track-2 item leaves track 1's selection untouched; (b) the edit cursor does not move on selection change; (c) per candidate action: toggle vs exclusive, which item it acts on, whether it works headless.
- Append to `docs/notes/cam-n-select.md` (it already holds the Decisions) a `## Holes` section with `H1` (does a native per-track toggle exist? record the id as an alternative only) and `H2` (cursor-at-item-end for a named track cannot be probed headless unless H1 gives a native; the unit test in step 2 is the arbiter). If a measurement contradicts a Decision in that doc, mark this step `- [!]` with a `BLOCKED:` line giving the values; do not guess.
- Do not write plugin code.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario cam_probe && grep -q "^## Holes" docs/notes/cam-n-select.md
```

- [x] Done

### 2. `CursorItemOnTrack`
- `plugin/src/target.{h,cpp}`: `int CursorItemOnTrack(const std::vector<ItemInfo> &items, int track, double cursor)` → item id or -1, applying the containment rule to one track (have `CursorTargetItem` reuse it if the rule overlaps).
- Tests in `plugin/test/target_test.cpp`: inside; boundary between two items → the one starting there; cursor at the last item's end → that item; before/after → -1; other track's item ignored.
- Do not add the verb yet.

**Verify**
```bash
tools/check target
```

- [ ] Done

### 3. Verb `select_track` + its LED provider
- `plugin/src/verb_registry.cpp`: free function `void SelectTrackTap(IProject &project, int n)` (next to `TimelineTap`: find the item with `CursorItemOnTrack`, flip its bit in the current selection, `SetSelectedItems`); verb class `SelectTrackVerb`; register `select_track` with gestures `{Tap}`, arg via `ParseIntArg`, range 1..9, and `state_read` per the Decisions in `docs/notes/cam-n-select.md`.
- Add `plugin/test/select_track_test.cpp` (register it in `plugin/test/CMakeLists.txt`): toggle on → off → on; additive (another track's selection kept); no item → no-op; cursor at last-item end → that item; arg 0 / 10 → parse error; `double`/`turn` rejected; `StateReadFor("select_track", {"2"})` true/false with selection and cursor moves; `{"x"}` → `nullopt`.
- Do not change `default_bindings.ini`; do not remove `take`/`CamSetTake`.

**Verify**
```bash
tools/check select_track
```

- [ ] Done

### 4. Default bindings: CAM1..9 → `select_track N`
- `plugin/src/default_bindings.ini`: replace the nine `take N` lines with `CAM1.tap = select_track 1` … `CAM9.tap = select_track 9`; comment that any LED-bearing key may be bound this way. Run `tools/gen_bindings_doc docs/bindings.md`.
- `plugin/test/default_bindings_test.cpp`: CAM entries → `select_track`.
- Add one `led_state_test` case through `BindingLedMask` with the real defaults: select the track-2 item under the cursor → CAM2's bit lit, no key press; CAM1 bound to `select_track 3` follows track 3; a non-CAM key (CLOSE_UP) bound to `select_track 2` lights its own bit.

**Verify**
```bash
tools/check 'default_bindings|led_state'
```

- [ ] Done

### 5. Oracle: `cam_select` replaces `leds_cam`
- Add `oracle/fake/cam_select.txt`, `oracle/scenarios/cam_select.{env,lua}` (default bindings; two tracks, one item each overlapping the cursor). Assert `led main=<hex>` each **within 1.5 s** (model: `leds_cam.lua`):
  - select the track-2 item via `B_UISEL` from Lua → CAM2 (bit 15, `0x8000`) lit with **no key press**;
  - fake tap CAM2 (`0x34` down/up) → item deselects, bit dark; tap again → lit;
  - move the edit cursor off/on the item via the REAPER API → bit follows.
- Second phase: write a `bindings.ini` (model: `bindings.lua`) binding CLOSE_UP to `select_track 2`; assert bit 0 follows track-2 state.
- Delete `oracle/fake/leds_cam.txt`, `oracle/scenarios/leds_cam.{lua,env}`; in `tools/reaper-oracle` `REAPER_SCENARIOS` replace `leds_cam` with `cam_select`.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario cam_select
```

- [ ] Done

### 6. Keep `take` e2e coverage in `simple_verbs`
- `oracle/scenarios/simple_verbs.lua`: before reload, write a `bindings.ini` with `CAM2.tap = take 2` (model: `bindings.lua`) so the take step still runs under the new defaults; update the header comment.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario simple_verbs
```

- [ ] Done

### 7. Docs: design + README
- `docs/design.md` §6: add `select_track | 1–9 | toggle selection of the item under the cursor on track N`; §6 CAM row and §10: CAM1…9 default-bind `select_track N`; any LED-bearing key bound to it lights iff that item is selected (lights without being pressed); `take` → lit only per 43's `take N` read.
- README "LED meanings": CAM1…CAM9 row → item under cursor on track N is selected (key bound to `select_track N`).

**Verify**
```bash
tools/check && grep -q 'select_track' docs/design.md && grep -q 'select_track' README.md
```

- [ ] Done

### 8. SMOKE-TEST
- `SMOKE-TEST.md`: add `## M45 — CAM N selection`: two-track setup; CAM N toggles the under-cursor item on track N, additive across tracks; select with the mouse or jog the cursor and the CAM N LED updates within ~1 s with no key press.

**Verify**
```bash
grep -q '^## M45' SMOKE-TEST.md && grep -q 'select_track\|CAM N' SMOKE-TEST.md
```

- [ ] Done
