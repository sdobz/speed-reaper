# Milestone 9 — Crossfade family (TRANS / DIS / CUT / TRANS DUR): spike first

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Builds on `34-m8-led-bar.md`. SMTH CUT shipped in M6 (`40916`). The rest of the family needs "crossfade at the cursor" and a default length, and **which REAPER API does that is unknown**. This file stops at a spike + human checkpoint. Do not guess. Design: `docs/plugin-design-m5-m9.md` §6; workflow `docs/reaper-plugin-workflow.md` §7. Every Verify starts with `tools/check`.

### 1. Spike: crossfade at the cursor, under the oracle

- Add `oracle/scenarios/spike_xfade.lua` (no device): two overlapping items A [0,5], B [4,9] on one track and two abutting items C [0,5], D [5,10] on another. Probe and `record` (do not assert behavior) for each: `D_FADEINLEN`, `D_FADEOUTLEN`, `D_FADEINLEN_AUTO`, `D_FADEOUTLEN_AUTO`, `C_FADEINSHAPE` before/after (a) `40916` with a time selection over the overlap, (b) setting the fade lengths by API on both items, (c) REAPER's auto-crossfade preference read (`SNM`-free: `GetConfigVar`/`get_config_var_string` for `autoxfade`-style names; see `docs/reaper-api-subset.md`). Also record how to read the "default crossfade length" (config var name) and whether it can be written.
- Also probe these action ids (names in `docs/reaper-actions.tsv`) as the "action id produced it" branch of the §2 rule: `41059` Crossfade any overlapping items; `43776` Set crossfade length to default (candidate for TRANS DUR tap); `43777` Set crossfade shape to default; `41191`/`41192`/`41193` Remove fade in / out / in and out (candidate for CUT); `40509`/`40510` Fade items in to / out from cursor; `41118`/`41119` Enable / disable auto-crossfades; `41116` cycle editing modes (also feeds M8 LEDs). Record each one's effect on the fade properties above.
- Write `docs/notes/m9-crossfade-api.md` from the output: a table per probe and 2–3 candidate designs for TRANS (create at cursor, abutting vs overlapping items), CUT (remove), TRANS DUR (tap default / double-tap save / hold+rotate). Do not implement any.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario spike_xfade --report-only >/dev/null; test -s docs/notes/m9-crossfade-api.md && grep -q '^| ' docs/notes/m9-crossfade-api.md
```

- [ ] Done

### 2. Decide the crossfade design by rule (no human)

- Write `docs/plugin-design-m9-crossfade.md` from `docs/notes/m9-crossfade-api.md`. Decision rule, apply in order and state which row won and why: (1) if the spike shows setting `D_FADEOUTLEN`/`D_FADEINLEN` (or the `_AUTO` pair) on two overlapping items yields a crossfade, use that; (2) else if an action id produced it, use the id; (3) else drop to "SMTH CUT only": TRANS/DIS/CUT/TRANS DUR show a message `crossfade needs a time selection, use SMTH CUT` and the doc says so.
- The doc must fix: names and signatures for pure functions in `plugin/src/crossfade.h`, the verb rows (TRANS and DIS = same verb; CUT = fade lengths 0 on the edge's two items; TRANS DUR tap = apply default length, double-tap = save the target edge's current length as default in `SetExtState("SpeedEditor","xfade.default_ms",…,true)`, hold+rotate = length ± 5 ms per step via the M7 `HoldNudge`), the default length (spike's measured REAPER default, else 10 ms), and the clamp (≥ 1 ms, ≤ overlap length).
- Do not implement.

**Verify**
```bash
test -s docs/plugin-design-m9-crossfade.md && grep -q 'Decision' docs/plugin-design-m9-crossfade.md
```

- [ ] Done

### 3. `Crossfade` math (pure)

- Create `plugin/src/crossfade.h/.cpp` exactly per the M9 design doc (edge detection between two `ItemGeom`s, overlap length, clamped fade lengths); add to `PLUGIN_PURE_SOURCES`. `plugin/test/crossfade_test.cpp`: overlapping pair, abutting pair, no neighbor, clamp to overlap, ≥ 1 ms.
- Do not edit `verbs.cpp`.

**Verify**
```bash
tools/check crossfade
```

- [ ] Done

### 4. TRANS / DIS / CUT / TRANS DUR rows

- Add composite rows per the M9 design doc, using `IProject::GetGeom/SetGeom` (M7) on the two items at the edge nearest the cursor within 0.5 s (target = `TargetItems` extended by its neighbor), one undo block each. Add `KEY_TRANS_DUR` to `VerbDoubleTapKeys()`. Tests in `verbs_test.cpp` with the fake project: each row's geometry changes, default save, no-neighbor does nothing.

**Verify**
```bash
tools/check verbs
```

- [ ] Done

### 5. Oracle scenario: `crossfade`

- `oracle/scenarios/crossfade.lua` + fake script + `.env`: overlapping A/B → TRANS yields the fade lengths the design specifies; CUT zeroes them; TRANS DUR hold+rotate changes the length by the expected steps; TRANS DUR double-tap then TRANS uses the saved default; ESC double-tap restores in one step. Fix wrong ids/fields in the design doc and record in `docs/notes/m5-observations.md`; do not loosen checks. Add to the default oracle run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario crossfade
```

- [ ] Done

### 6. Test instructions: SMOKE-TEST.md "M9 — crossfades"

- Write the hand-test section for what the oracle cannot judge: audible click-free transition at the default length, TRANS DUR rotate feel (step size comfortable? list the parameter to change if not), E3 end to end. Each step: action, expected result, and a pass/fail box. If the design doc took fallback (3), say which buttons only show a message.

**Verify**
```bash
grep -q '^## M9 — crossfades' SMOKE-TEST.md
```

- [ ] Done
