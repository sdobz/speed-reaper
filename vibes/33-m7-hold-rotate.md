# Milestone 7 — Hold + rotate: MOVE, edge nudge, SLIP, volume

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Builds on `32-m6-simple-verbs.md`. The gesture engine already emits `HoldStart` / `HoldRotate` / `HoldEnd` (M3); no verb uses them. Design (normative): `docs/plugin-design-m5-m9.md` §4. One hold = one undo block; the target set is resolved once at `HoldStart`. Every Verify starts with `tools/check`.

### 1. `HoldNudge` accumulator (pure)

- Create `plugin/src/hold_nudge.h/.cpp`: `class HoldNudge { explicit HoldNudge(int ticks_per_step = 4); int Add(std::int32_t delta); }` — returns the whole steps (signed) completed by the delta, keeping the remainder (negative and positive remainders symmetric toward zero). Add to `PLUGIN_PURE_SOURCES`.
- `plugin/test/hold_nudge_test.cpp`: 3 ticks → 0, +1 more → 1; −4 → −1; 10 ticks → 2 steps remainder 2, then 2 → 1; direction reversal does not lose or invent steps.
- Do not edit `verbs.cpp`.

**Verify**
```bash
tools/check hold_nudge
```

- [x] Done

### 2. `IProject` item geometry

- Add to `IProject` (+ fake + `ReaperProject`): `double Fps()` (project frame rate via `TimeMap_curFrameRate`, 30.0 if unknown), `ItemGeom GetGeom(int id)` / `void SetGeom(int id, const ItemGeom&)` with `struct ItemGeom { double pos, len, startoffs, playrate, vol; }` (`D_POSITION`, `D_LENGTH`, take `D_STARTOFFS`, `D_PLAYRATE`, item `D_VOL`; `UpdateItemInProject`).
- Do not add verbs.

**Verify**
```bash
tools/check
```

- [x] 2. `IProject` item geometry
  <!-- ralph: replanned=1 -->
  - Add `struct ItemGeom { double pos, len, startoffs, playrate, vol; }` to `plugin/src/project.h`; add `double Fps()`, `ItemGeom GetGeom(int id)`, `void SetGeom(int id, const ItemGeom&)` to `IProject`, `FakeProject` (`plugin/test/fake_project.h`), and `ReaperProject` (`plugin/src/reaper_project.h/.cpp`).
  - Reaper impl: pos/len/vol via `Get/SetMediaItemInfo_Value` (`D_POSITION`, `D_LENGTH`, `D_VOL`); startoffs/playrate via `GetActiveTake(item)` + `GetMediaItemTakeInfo_Value`/`SetMediaItemTakeInfo_Value` (`D_STARTOFFS`, `D_PLAYRATE`) — these 7.x names are in `vendor/reaper-sdk/sdk/reaper_plugin_functions.h`; there is no `GetMediaTakeInfo_Value` here, do not go looking. Call `UpdateItemInProject` after writing. `Fps()` = `TimeMap_curFrameRate`; return 30.0 when it is not > 0.
  - Add each new `REAPERAPI_WANT_` line to `reaper_project.cpp` AND mirror it in `main.cpp` (see the comment above main.cpp's WANT list); `UpdateItemInProject` is already mirrored.
  - Fake: `std::vector<ItemGeom> geoms` index-aligned with `items` (missing entry = `ItemGeom{0,0,0,1,1}`); GetGeom merges `items[id]` pos/len with `geoms[id]`, SetGeom writes both; add `double fps = 30.0` member; out-of-range id is a no-op.
  - Do not add verbs.
  Verify: `tools/check`

### 3. Pure nudge math

- Create `plugin/src/nudge.h/.cpp`: free functions per design §4 table, each `ItemGeom Apply(const ItemGeom&, int steps, double step_seconds)`: `NudgeMove`, `NudgeTrimIn`, `NudgeTrimOut`, `NudgeSlipSrc`, `NudgeSlipDest`, `NudgeVolume` (0.5 dB/step, clamp 0…+12 dB; `steps` are signed). Clamps: length ≥ one step, `startoffs` ≥ 0. Add to `PLUGIN_PURE_SOURCES`.
- `plugin/test/nudge_test.cpp`: one assertion pair per function including the clamps and `playrate != 1`.

**Verify**
```bash
tools/check nudge
```

- [x] Done

### 4. Hold rows in `VerbRouter`

- Handle `HoldStart` (resolve targets once via `TargetItems`, snapshot selection, `Undo_Begin`), `HoldRotate` (`HoldNudge.Add(delta)` → apply the key's function to every target, step = `1/Fps()`), `HoldEnd` (restore selection, `Undo_End("Speed Reaper: <verb>")`). Keys: SPLIT, TRIM_IN, TRIM_OUT, SLIP_SRC, SLIP_DEST, AUDIO_LEVEL per design §4. Unmapped keys and no-target holds do nothing (no undo block opened).
- Extend `verbs_test.cpp` with a fake project: SPLIT hold + 8 ticks → item `pos` +2 frames, exactly one `Undo_Begin`/`Undo_End` pair; hold with no target → none; release restores selection.
- Check `main.cpp` already sends wheel deltas to the gesture engine while `HoldActive()` (design §7) and fix only if it does not.

**Verify**
```bash
tools/check verbs
```

- [x] Done

### 5. Oracle scenarios for hold + rotate (5a–5c)

#### 5a. Scenario `hold_move_trim`: SPLIT and TRIM OUT holds

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules: rebuild first, defer errors, two-attempt limit).
- Item A [0,5] on a track, nothing selected, cursor 2.5, project 30 fps. The fake grammar needs wheel events while a key is down; read `oracle/fake/wheel_turn.txt` and `oracle/fake/buttons.txt` for the syntax. Add `oracle/fake/hold_move_trim.txt` + `.env` + `hold_move_trim.lua` and the name to `REAPER_SCENARIOS` in `tools/reaper-oracle`.
- Hold SPLIT + 8 ticks → A.pos≈ 2/30; hold TRIM OUT + −8 ticks → A.len reduced by 2/30. Selection stays empty throughout.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario hold_move_trim
```

- [x] Done

#### 5b. Scenario `hold_slip_vol`: SLIP SRC and AUDIO LEVEL holds

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules: rebuild first, defer errors, two-attempt limit).
- Item A [0,5] on a track, nothing selected, cursor 2.5, project 30 fps. The fake grammar needs wheel events while a key is down; read `oracle/fake/wheel_turn.txt` and `oracle/fake/buttons.txt` for the syntax. Add `oracle/fake/hold_slip_vol.txt` + `.env` + `hold_slip_vol.lua` and the name to `REAPER_SCENARIOS` in `tools/reaper-oracle`.
- Hold SLIP SRC + 4 ticks → `D_STARTOFFS`≈ 1/30 with pos/len unchanged; hold AUDIO LEVEL + 4 ticks → `D_VOL`≈ 10^(0.5/20). Selection stays empty.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario hold_slip_vol
```

- [x] Done

#### 5c. Scenario `hold_undo`: ESC restores a hold in one undo step

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules: rebuild first, defer errors, two-attempt limit).
- Item A [0,5] on a track, nothing selected, cursor 2.5, project 30 fps. The fake grammar needs wheel events while a key is down; read `oracle/fake/wheel_turn.txt` and `oracle/fake/buttons.txt` for the syntax. Add `oracle/fake/hold_undo.txt` + `.env` + `hold_undo.lua` and the name to `REAPER_SCENARIOS` in `tools/reaper-oracle`.
- Hold SPLIT + 8 ticks, then ESC double-tap → A restored to its original pos in **one** undo step (one undo-count change, not eight).

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario hold_undo
```

- [x] Done

### 6. SMOKE-TEST.md: "M7 — hold + rotate"

- Add the section: E3 hold-SPLIT MOVE, TRIM OUT hold+wheel in E5 (tape-mode part waits for M8), SLIP SRC feel (audible content slide), AUDIO LEVEL hold+wheel dB feel, one-ESC-undo per gesture. Note "not ripple-aware" (design §4). Expected results per step.

**Verify**
```bash
grep -q '^## M7 — hold' SMOKE-TEST.md
```

- [x] Done
