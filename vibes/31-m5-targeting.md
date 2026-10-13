# Milestone 5 — Targeting without a select button: RIPL DEL & friends work on a bare timeline

Builds on `22-m3-edit-verbs.md`. REAPER's item actions (`Item: Remove items`, trim) act on *selected items*, and the Speed Editor has no way to select. This milestone adds the implicit-target rule, an explicit select verb on TIMELINE, and a RIPL DEL that really ripples. Design (normative): `docs/plugin-design-m5-m9.md` §1–§3. Oracle rules: `docs/plugin-oracle-design.md`. Every Verify starts with `tools/check`.

### 1. `IProject` + `TargetItems` (pure)

- Create `plugin/src/project.h` (exactly the interface in design §2) and `plugin/src/target.h/.cpp` with `std::vector<int> TargetItems(IProject&)` per design §1.1 and `template<class F> void WithTargetSelected(IProject&, F f)` (snapshot selection, select targets only in rule 2, call `f`, restore). Add `target.cpp` to `PLUGIN_PURE_SOURCES`.
- Add `plugin/test/fake_project.h` (a configurable `IProject` fake recording `SetSelectedItems` calls) and `plugin/test/target_test.cpp`: selected items win; none selected + cursor inside an item → that one; cursor at item end of last item → that item; two items on the track, cursor on a boundary → the one starting there; no track selected → last-touched track; nothing under cursor → empty; `WithTargetSelected` restores the original selection afterwards (including "none selected").
- Do not implement `ReaperProject`. Do not edit `main.cpp` or `verbs.cpp`.

**Verify**
```bash
tools/check target
```

- [x] Done

### 2. `ReaperProject` (REAPER-backed `IProject`)

- Create `plugin/src/reaper_project.h/.cpp` implementing `IProject` per design §2 using REAPER API calls (look each up in `docs/reaper-api-subset.md`; add a `REAPERAPI_WANT_` line for each in `main.cpp`'s existing list). Ripple read: `GetToggleCommandState(40310)` → 1, `(40311)` → 2, else 0. Undo: `Undo_BeginBlock2(nullptr)` / `Undo_EndBlock2(nullptr, label, -1)`. Keep it out of `PLUGIN_PURE_SOURCES` (it needs the REAPER API); add it to the plugin target only.
- Do not wire it into `VerbRouter` yet. No tests in this step (step 5's oracle covers it).

**Verify**
```bash
tools/check
```

- [x] Done

### 3. `VerbRouter` takes `IProject`; composite rows

- Change `VerbRouter` to `VerbRouter(IActionSink&, IProject&)`; add a composite-verb table next to `kVerbTable` in `verbs.cpp` (design §2); composite rows win over id rows. Update `main.cpp` (construct a `ReaperProject`) and `verbs_test.cpp` (use `fake_project.h`); existing tests must still pass unchanged in meaning.
- Move TRIM IN / TRIM OUT (`40511` / `40512`) from the id table to composite rows that run the same ids inside `WithTargetSelected` + one undo block labelled `Speed Reaper: trim`.
- Do not implement RIPL DEL or TIMELINE yet.

**Verify**
```bash
tools/check verbs
```

- [ ] Done

### 4. RIPL DEL composite

- In `verbs.cpp`, replace the RIPL DEL id row with a composite per design §3: `project.HasTimeSelection()` → `RunCommand(40201)`; else `WithTargetSelected` + one undo block: if `RippleMode()==0` run `40310`, `40006`, `40309`; else just `40006`. No targets → trace `target none`, do nothing.
- Extend `verbs_test.cpp`: time selection → exactly `[40201]`; ripple off + one target → `[40310, 40006, 40309]`; ripple per-track → `[40006]`; no target and no time selection → no commands; selection restored after each.
- Do not touch the oracle scenarios here.

**Verify**
```bash
tools/check verbs
```

- [ ] Done

### 5. TIMELINE select verbs

- Per design §1.2 add composite rows: TIMELINE Tap (select target exclusively; if that is already the sole selection → deselect all), DoubleTap (`40289`), HoldRotate (accumulate `delta`; per `kSelectStepTicks`=20 move selection to next/previous item on the target track, clamped at the ends, cursor untouched). Add TIMELINE to `VerbDoubleTapKeys()` and make sure `main.cpp` still routes TIMELINE to the gesture engine, not the Router (it is not a wheel-mode key).
- Extend `verbs_test.cpp` for the three gestures (fake project: selection results via recorded `SetSelectedItems`).

**Verify**
```bash
tools/check verbs
```

- [ ] Done

### 6. Oracle scenario: `target` (proves the ids and the behavior)

- Add `oracle/fake/target.txt` + `oracle/scenarios/target.env` + `target.lua` (same shape as `verbs.*`; reuse the grammar of `oracle/fake/verbs.txt`). Setup in Lua: one track with adjacent items A [0,5], B [5,10], **nothing selected**, cursor 2.5, ripple off. Steps and assertions: RIPL DEL (0x2B) → A gone, B now at 0 (gap closed even though ripple was off), ripple mode back to off, selection unchanged (none); undo (ESC double-tap) restores A and B; IN/OUT around [1,2] on A then RIPL DEL → time selection removed and B shifted by 1; TRIM IN at cursor with nothing selected → A's left edge at cursor, selection unchanged; TIMELINE tap → A selected, tap again → none selected; TIMELINE double-tap → none selected.
- A failing step means a wrong id or bug: fix it and record the corrected id + proving check in `docs/notes/m5-observations.md`. Do not loosen a check. Add the scenario to the default oracle run (as done for `verbs`).

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario target
```

- [ ] Done

### 7. SMOKE-TEST.md: "M5 — targeting"

- Add section "M5 — targeting" after "M3": two adjacent items, nothing selected; RIPL DEL deletes the item at the cursor and closes the gap (try ripple off — still closes — and note that ripple stays off afterwards); IN/OUT + RIPL DEL; TRIM IN with no selection; TIMELINE tap/double-tap/hold+rotate selection stepping; ESC double-tap undoes a RIPL DEL in **one** step. List expected results per step.

**Verify**
```bash
grep -q '^## M5 — targeting' SMOKE-TEST.md && grep -c 'Expected' SMOKE-TEST.md
```

- [ ] Done
