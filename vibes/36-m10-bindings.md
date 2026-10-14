# Milestone 10 — Configurable non-obvious buttons (binding table)

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Builds on `34-m8-led-bar.md` (needs composite rows and `IProject`; independent of M9). The non-obvious buttons (SMART INSRT, APPND, RIPL O/WR, SRC O/WR, CLOSE UP, SYNC BIN, LIVE O/WR, SOURCE, ROLL, and the crossfade keys until M9 lands) ship **unassigned** and are bound by the user. Design (normative): `docs/plugin-design-m5-m9.md` §6; storage rules: `docs/plugin-configuration.md` §3.3–3.4. Every Verify starts with `tools/check`.

### 1. `BindingTable` (pure)

- Create `plugin/src/bindings.h/.cpp`: `struct Binding { std::string command; bool targeted; }`; `class BindingTable { void Set(std::string key, std::string value); const Binding* Find(std::uint16_t key, GestureKind kind) const; }`. Keys `<KEYNAME>.<gesture>` with `KEYNAME` = the `KEY_*` suffix (`SYNC_BIN`, `CLOSE_UP`, `APPND`, …), gesture ∈ `tap|double|hold`; values per design §6 (`T:` prefix → `targeted`). Only the non-obvious key set is accepted (`Set` of any other key returns false, table unchanged); unparsable values are ignored. Add to `PLUGIN_PURE_SOURCES`.
- `plugin/test/bindings_test.cpp`: parse numeric, `_RSabc`, `T:40006`; reject `SPLIT.tap`; unknown gesture; empty value clears.
- Do not edit `verbs.cpp` or `main.cpp`.

**Verify**
```bash
tools/check bindings
```

- [ ] Done

### 2. Resolve named commands: `IActionSink::RunNamed`

- Add `virtual void RunNamed(const std::string& command, bool targeted) = 0` semantics to `IActionSink`: numeric string → `Main_OnCommand`, otherwise `NamedCommandLookup`; unresolved → trace `bind unresolved <command>` and do nothing. Implement in `main.cpp`'s sink and the test fakes (the fake records the string).
- Do not load config yet.

**Verify**
```bash
tools/check
```

- [ ] Done

### 3. Load bindings at startup and consult them in `VerbRouter`

- In `main.cpp` read every key of the non-obvious set × gestures with `GetExtState("SpeedEditor", key)` (empty = unassigned) into a `BindingTable` passed to `VerbRouter`; `OnGesture` checks the table first for those keys; `targeted` bindings run through `WithTargetSelected` and one undo block. Add the bound keys that need a second tap (APPND, SMART_INSRT, …) to the double-tap set only when a `.double` binding exists.
- Test in `verbs_test.cpp`: bound `SYNC_BIN.tap=_RSabc` → fake records `_RSabc`; bound `APPND.tap=T:40006` → selection snapshot/restore around it; unbound non-obvious key → nothing; built-in keys unaffected by a stray table entry.

**Verify**
```bash
tools/check verbs
```

- [ ] Done

### 4. Reload action `speed_reaper_reload_bindings`

- Register an action ("Speed Reaper: reload bindings") that re-reads the ExtState keys into the live `BindingTable` (extract the startup load from step 3 into one function both call); trace `bindings loaded n=<count>`.

**Verify**
```bash
tools/check && strings build/plugin/reaper_speed_reaper.so | grep -q speed_reaper_reload_bindings
```

- [ ] Done

### 5. Oracle scenario: `bindings`

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules: rebuild first, defer errors, two-attempt limit).
- Add `oracle/scenarios/bindings.lua` + fake device script + `.env`: Lua sets `SetExtState("SpeedEditor","APPND.tap","40157",true)` and `("CLOSE_UP.tap","T:40006",true)`, runs the reload action (`NamedCommandLookup`), then the fake presses APPND (marker count +1), CLOSE UP with an unselected item under the cursor (item removed, selection unchanged) and SYNC BIN (unbound → no `cmd` trace).

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario bindings
```

- [ ] Done

### 6. Bind-by-learn action (optional UI)

- Register `speed_reaper_bind` ("Speed Reaper: bind next pressed device key to the selected action"): per `docs/plugin-configuration.md` §3.3 steps 2–4 using `PromptForAction`; after the action is chosen, the next non-obvious key tap stores `<KEY>.tap` (re-tap = clear) via `SetExtState(..., true)` and reloads the table; any other key shows a message and cancels. Only the non-obvious set is accepted. `PromptForAction` cannot be driven headless, so write the hand-test instructions (the checklist) in SMOKE-TEST.md "M10 — bindings": bind SYNC BIN to a custom action, fire it, restart REAPER → still bound; clear it; delete `[SpeedEditor]` from `reaper.ini` → all unbound. Expected results per step.

**Verify**
```bash
tools/check && grep -q '^## M10 — bindings' SMOKE-TEST.md
```

- [ ] Done
