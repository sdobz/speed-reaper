# Milestone 3 — Edit verbs: a usable rough-cut device

Builds on `21-m2-buttons-modes.md`. Adds the gesture engine (tap, double-tap,
hold) and the first tap-only editing verbs — SPLIT, RIPL DEL, IN/OUT, TRIM,
SNAP (+LED), ESC/UNDO — enough to do a real audio rough edit on the device.
Hold+rotate verbs and the ripple LED are backlog (`30-backlog.md`), not here.
Design: `docs/plugin-design-m1-m3.md` §7. Verified by the oracle (`docs/plugin-oracle-design.md`); the oracle also settles the command ids by behavior. Every Verify starts with `tools/check`.

### 1. `GestureEngine` (pure)

- Create `plugin/src/gesture.h/.cpp` exactly per design §7 (types, signatures, rules) and add it to `PLUGIN_PURE_SOURCES`. Time is passed in (`now_ms`); no clocks inside.
- Add `plugin/test/gesture_test.cpp` covering: quick press/release → one `Tap` (non-double key: emitted on release); double-tap key tap → nothing until `Poll` past `double_tap_ms`, then one `Tap`; two taps in the window → exactly one `DoubleTap` and no `Tap`; hold past `hold_ms` (via `Poll`) → `HoldStart` once, release → `HoldEnd` and no `Tap`; wheel delta while held → `HoldStart` (if not yet) then `HoldRotate` with the delta; `HoldActive()` true between start and end.
- Do not write the verb table yet. Do not edit `main.cpp`.

**Verify**
```bash
tools/check gesture
```

- [x] Done

### 2. `VerbRouter` and the command-id table (pure)

- Create `plugin/src/verbs.h/.cpp` per design §7: one table at the top of `verbs.cpp` mapping (key, kind) → command id, exactly the rows in the design table, using `KEY_*` from `key_map.h`. `OnGesture` runs the command via `IActionSink::RunCommand`; unmapped gestures do nothing. Also export `std::vector<std::uint16_t> VerbDoubleTapKeys()` (IN, OUT, ESC) for `SetDoubleTapKeys`. Add `verbs.cpp` to `PLUGIN_PURE_SOURCES`.
- Add `plugin/test/verbs_test.cpp` with a fake sink: SPLIT Tap → 40012; ESC Tap → 40635; ESC DoubleTap → 40029; IN Tap → 40625; IN DoubleTap → 40635; unmapped key and HoldRotate → nothing.
- Do not edit `main.cpp`.

**Verify**
```bash
tools/check verbs
```

- [x] Done

### 3. Wire gestures + verbs into the plugin

- In `plugin/src/main.cpp`: a `GestureEngine` (call `SetDoubleTapKeys(VerbDoubleTapKeys())`) and `VerbRouter` using the existing `ReaperActionSink`. In `onTimer()`: for each `ButtonChange` from `g_hid.PopButton()`, **if the key is a Router key (JOG/SCRL/SHTL/STOP-PLAY) send it only to `Router`, otherwise only to `GestureEngine::OnKey`** (`now_ms` from `std::chrono::steady_clock`). Trace `cmd <id>` for every command (already done by the action sink) and `gesture <key hex> <kind>` for every popped gesture. Call `Poll(now)` every tick; `Pop` every gesture into `VerbRouter::OnGesture`.
- Behavior of wheel and modes is unchanged (hold+rotate deltas are not routed yet).

**Verify**
```bash
tools/check
```

- [x] Done

### 4. SNAP LED follows REAPER's snap state

- Reading the snap state: `GetToggleCommandState(1157)` returns 1 when snapping is on (REAPER main section). Add `plugin/src/led_state.h/.cpp` (pure): `std::uint32_t MainLedMask(bool snapOn)` returns the main-panel LED bitmask with only `LED_SNAP` (bit index from `key_map.h`) set when on.
- Add `plugin/test/led_state_test.cpp` (on → `1u<<LED_SNAP`, off → 0).
- In `main.cpp`'s timer: every ~250 ms (count ticks), read `GetToggleCommandState(1157)` and, when the mask changed (or after a reconnect edge), call `g_hid.RequestLeds(mask)`.
- Trace `led main=<hex>` on every `RequestLeds` call; add `REAPERAPI_WANT_GetToggleCommandState`.
- Do not add other LEDs. Do not read the ripple state.

**Verify**
```bash
tools/check led_state
```

- [ ] Done

### 5. Oracle scenario: `verbs` (also validates the command ids)

- Add `oracle/fake/verbs.txt` driving, 400 ms apart: SPLIT `0x2F`; IN `0x07`; OUT `0x08`; IN double-tap; TRIM IN `0x09`; RIPL DEL `0x2B`; SNAP `0x2E`; ESC `0x31` double-tap. (If a step needs state a previous step changed, reorder; document the order in a comment in the script.)
- Add `oracle/scenarios/verbs.env` + `verbs.lua`: new project; one **empty item** (`AddMediaItemToTrack`, position 0, length 10 s) on a track, selected, edit cursor at 5 s. Per step, assert the REAPER-visible effect: item count 1→2 after SPLIT; `GetSet_LoopTimeRange` start≈5 after IN and end≈5 after OUT, cleared after IN double-tap; trimmed item edge at the cursor after TRIM IN; item count drops after RIPL DEL; `GetToggleCommandState(1157)` flips after SNAP and `led main=` is traced; `Undo_CanUndo2` string changes after ESC double-tap and no `cmd 40635` follows the undo.
- **A failing step means a wrong command id (or a bug).** Fix the id in the `verbs.cpp` table, then record each corrected id and the check that proved it in `docs/notes/m3-observations.md`. Do not loosen a check to pass.
- Add the scenario to the default oracle run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario verbs
```

- [ ] Done

### 6. SMOKE-TEST.md: "M3 — edit verbs"

- Append `## M3 — edit verbs` to `SMOKE-TEST.md` (numbered steps with **Expected**), using a project with two audio items on a track:
  1. SPLIT at the edit cursor splits the selected item (same as REAPER's `S`).
  2. IN, OUT set the time selection start/end at the cursor; double-tap IN (or OUT) clears it; ESC tap clears it.
  3. TRIM IN / TRIM OUT trim the selected item's start/end to the cursor.
  4. RIPL DEL deletes the selected item; with ripple editing on (REAPER's `Alt+P` cycles modes, or the Options menu), the gap closes; with it off, it does not.
  5. SNAP toggles REAPER's snap; its LED matches REAPER's state; toggling snap from REAPER's own toolbar updates the LED within ~1 s.
  6. ESC double-tap = one Undo; it does not also fire the ESC tap (time selection not cleared by the undo gesture).
  7. Record in `docs/notes/m3-observations.md` how the double-tap window and hold feel on the real device (are 300/350 ms right?). The command ids were already verified by the oracle `verbs` scenario.
- Do not change other sections.

**Verify**
```bash
grep -q '^## M3 — edit verbs' SMOKE-TEST.md
```

- [ ] Done

### 7. (human) Run the M3 hand test

- [!] Done
  BLOCKED: precondition — `ORACLE_FULL=1 tools/check` green. Human runs SMOKE-TEST.md "M3" with the real device and records timing feel in `docs/notes/m3-observations.md`. Pass bar: do a real rough cut of an audio clip (split, trim, delete, undo) entirely from the device while the wheel jogs. Then tick this box; next is `23-m4-shuttle.md`.
