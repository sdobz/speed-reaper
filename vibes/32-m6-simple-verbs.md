# Milestone 6 — Obvious tap verbs: takes, marker, SMTH CUT, PLACE ON TOP, view buttons

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

### 6. Oracle scenario: `simple_verbs`

- Add `oracle/fake/simple_verbs.txt` + `.env` + `simple_verbs.lua`. Setup: track 1 with item A [0,5] holding two takes, track 2 with item B [0,5] one take, nothing selected, cursor 1. Assertions: CAM2 → A's `I_CURTAKE`==1, B's 0; AUDIO LEVEL double-tap → marker count +1 at cursor; time selection [1,2] on overlapping A/C + SMTH CUT → `GetMediaItemInfo_Value(C,"D_FADEINLEN_AUTO")` or the crossfade the build produces is >0 (record what you measure); PLACE ON TOP → track count +1 and item count +1; AUDIO_ONLY twice → solo flag set then cleared; VIDEO_ONLY twice → track 2 hidden then shown; FULL VIEW → trace `cmd 40346` and the command exists (`GetToggleCommandState` not -1). Fix wrong ids in the tables and record them in `docs/notes/m5-observations.md`; do not loosen checks. Add to the default oracle run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario simple_verbs
```

- [ ] Done

### 7. SMOKE-TEST.md: "M6 — takes, marker, view"

- Add the section: E6 (three-take item, CAM 2/3, nothing selected vs. selected), E8 (MARK), SMTH CUT over a jump cut, PLACE ON TOP, VIDEO_ONLY/AUDIO_ONLY re-tap restores, FULL VIEW. List expected results per step (CAM LEDs are M8; note "LED not yet" for CAM).

**Verify**
```bash
grep -q '^## M6 — takes' SMOKE-TEST.md
```

- [ ] Done
