# Milestone 6 — Obvious tap verbs: takes, marker, SMTH CUT, PLACE ON TOP, view buttons

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`; REAPER 7.82 action list, Main section = ids for `Main_OnCommand`). e.g. `rg -i 'crossfade' docs/reaper-actions.tsv`. Never guess an id.

Builds on `31-m5-targeting.md` (needs `IProject`, `WithTargetSelected`, composite rows). Each button here has a direct REAPER equivalent. Design (normative): `docs/plugin-design-m5-m9.md` §3 table. Every Verify starts with `tools/check`. One task = one verb group; the oracle scenario is the last task and proves the ids.

### 1. Extend `IProject` for takes

- Add to `IProject` (and `fake_project.h`, `ReaperProject`): `int ItemTakeCount(int id)`, `int ItemActiveTake(int id)` (0-based), `void SetItemActiveTake(int id, int take)` (`I_CURTAKE` + `UpdateItemInProject`).
- Do not add verbs yet.

**Verify**
```bash
tools/check
```

- [x] Done

### 2. CAM 1–9 = active take N

- Composite rows for `KEY_CAM1`…`KEY_CAM9` per design §3: `WithTargetSelected` + undo block `Speed Reaper: take N`; targets with fewer than N takes keep theirs. Write the row generation as a loop, not nine copies.
- Add tests to `verbs_test.cpp`: CAM2 on two targets (3 takes, 1 take) → first set to index 1, second untouched; CAM1 with no target → nothing.
- Do not do LEDs (M8).

**Verify**
```bash
tools/check verbs
```

- [x] Done

### 3. MARK, level read-out, SMTH CUT

- Rows: AUDIO LEVEL DoubleTap → `40157` (add `KEY_AUDIO_LEVEL` to `VerbDoubleTapKeys()`); AUDIO LEVEL Tap → composite: message box (`ShowConsoleMsg` via a new `IActionSink::Message(const char*)`; add to the fake sink) of `Volume: <x.x> dB` for the first target item (`20*log10(D_VOL)`; add `ItemVolume(id)` to `IProject`); SMTH CUT Tap → `40916` if `HasTimeSelection()` else trace `no time selection`.
- Tests: the three rows above (message text for D_VOL=0.5 → `-6.0 dB`).

**Verify**
```bash
tools/check verbs
```

- [x] Done

### 4. PLACE ON TOP

- Add `IProject::DuplicateItemsAbove(const std::vector<int>& ids)`; `ReaperProject` inserts one new track above each source track, copies each item (`AddMediaItemToTrack` + `GetItemStateChunk`/`SetItemStateChunk`, same position) and leaves the original selection alone. Composite row PLACE ON TOP Tap: `WithTargetSelected`-style target resolution, one undo block `Speed Reaper: place on top`.
- Test with the fake project (one call with the target ids). No oracle here.

**Verify**
```bash
tools/check verbs
```

- [x] Done

### 5. View buttons: isolate, solo, maximize

- `IProject::ToggleIsolateTracks(const std::vector<int>& itemIds)` and `ToggleSoloTracks(...)` — each saves the previous per-track flags (`B_SHOWINTCP`/`B_SHOWINMIXER`, `I_SOLO`) on the first call and restores exactly those on the second call; state is plugin-memory only. VIDEO_ONLY → isolate, AUDIO_ONLY → solo, FULL VIEW → `RunCommand(40346)` (candidate, settled by step 6).
- Tests with the fake project: each toggle called with the target ids.

**Verify**
```bash
tools/check verbs
```

- [x] Done

### 6. Oracle scenarios for M6 (split into 6a–6d)

One scenario per behaviour. Shared rules for every 6x task: `docs/notes/oracle-scenario-rules.md` (read it first).

#### 6a. Scenario `simple_verbs`: CAM 2, AUDIO LEVEL double-tap, FULL VIEW

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules).
- Rewrite `oracle/scenarios/simple_verbs.lua` and `oracle/fake/simple_verbs.txt` down to three gestures: CAM 2 `0x34` tap, AUDIO LEVEL `0x2C` double-tap, FULL VIEW `0x2D` tap. Drop PLACE ON TOP, SMTH CUT, AUDIO_ONLY, VIDEO_ONLY from both files (they get their own scenarios).
- Setup: track "A" with item A [0,5] holding two takes, track "B" with item B [0,5] one take, cursor 1, A's track pinned as last-touched (as in the current file). No tone sources needed.
- Checks: A `I_CURTAKE`==1 and B's 0; marker count +1 at cursor 1; `GetToggleCommandState(40346) ~= -1`; trace gestures are CAM2, AUDL double-tap, FULL; trace cmd lines are `40157`, `40346`.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario simple_verbs
```

- [x] Done

#### 6b. Scenario `smth_cut`: SMTH CUT crossfades a meeting boundary

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules).
- New scenario, one gesture: SMTH CUT `0x11` tap (cmd `40916`).
- Setup directly through the API, no PLACE ON TOP: one track, item A [0,1.5] and item C [1.5,5] meeting at 1.5, tone PCM sources on both (copy `le16`/`le32`/`write_tone` from `oracle/scenarios/probe_sv4.lua`), both items selected via 40717 as in probe_sv4, time selection [1,2]. This is the proven working layout: it gave `D_FADEOUTLEN_AUTO`/`D_FADEINLEN_AUTO` ≈ 0.99.
- Check: after the gesture some item's `D_FADEINLEN_AUTO` or `D_FADEOUTLEN_AUTO` > 0 (record the value), and the trace has `cmd 40916`.
- Then delete `oracle/scenarios/probe_*.lua` files that are no longer referenced by any doc, and add `smth_cut` to `REAPER_SCENARIOS` in `tools/reaper-oracle`.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario smth_cut
```

- [x] Done

#### 6c. Scenario `place_on_top`: PLACE ON TOP duplicates the target

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules).
- New scenario, one gesture: PLACE ON TOP `0x05` tap.
- Setup: reuse the target setup from `oracle/scenarios/target.lua` (an item that resolves as the target). No tone sources, no time selection.
- Checks: track count +1 and item count +1 after the gesture (count items by looping `GetTrackMediaItem`), and the trace shows the PLACE ON TOP gesture.
- Add `place_on_top` to `REAPER_SCENARIOS`.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario place_on_top
```

- [x] Done

#### 6d. Scenario `view_solo`: AUDIO_ONLY and VIDEO_ONLY re-tap restores

- First read `docs/notes/oracle-scenario-rules.md` (shared facts and rules).
- New scenario, four gestures: AUDIO_ONLY `0x26` twice, then VIDEO_ONLY `0x25` twice.
- Setup: two tracks with one item each, target as in `target.lua`. Track selection via `B_SELECTED` writes is unreliable in this build; use the commands from `target.lua` to make the target, and make the assertions independent of WHICH track is the target:
  - AUDIO_ONLY: sum of `I_SOLO` over tracks is 0 → ≥1 → 0.
  - VIDEO_ONLY: number of tracks with `B_SHOWINTCP`==1 goes N → fewer → N.
- Add `view_solo` to `REAPER_SCENARIOS`. Then run the whole suite once to confirm nothing else regressed.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario view_solo && tools/reaper-oracle --layer reaper
```

- [x] Done

### 7. SMOKE-TEST.md: "M6 — takes, marker, view"

- Add the section: E6 (three-take item, CAM 2/3, nothing selected vs. selected), E8 (MARK), SMTH CUT over a jump cut, PLACE ON TOP, VIDEO_ONLY/AUDIO_ONLY re-tap restores, FULL VIEW (`40346` is REAPER's "Toggle fullscreen", not "maximize arrange"; no such action exists in `docs/reaper-actions.tsv` — record what it actually does). List expected results per step (CAM LEDs are M8; note "LED not yet" for CAM).

**Verify**
```bash
grep -q '^## M6 — takes' SMOKE-TEST.md
```

- [x] Done
