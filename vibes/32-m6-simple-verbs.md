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
  <!-- ralph: replanned=1 op=AND -->
  - [ ] Spike: settle the SMTH CUT command (does 40916 crossfade?) — findings to docs/notes/m5-observations.md
    Finish oracle/scenarios/probe_smth.lua (attempt 6's throwaway probe; it crashes at reaper.Main_NewProject, which is NOT in this Lua build — never reset the project, use separate tracks per config) to test what SMTH CUT does empirically. Configs: (a) task's — two [0,5] items on adjacent tracks fully overlapping + time selection [1,2] via GetSet_LoopTimeRange(true,true,1,2,true); (b) canonical per docs/reaper-plugin-workflow.md — two MEETING items on one track within the time selection. Run Main_OnCommand(40916), dump per-item positions + D_FADEINLEN_AUTO/D_FADEOUTLEN_AUTO before/after; if 40916 is a no-op, fetch https://raw.githubusercontent.com/madskjeldgaard/reaper-nvim/master/lua/reaper-nvim/actions.lua, grep -i 'crossfade\|fade' for candidate ids, probe the top candidates the same way. Also settle: capture ALL 8 returns of reaper.EnumProjectMarkers(i) after a 40157 marker insert (marker lands at pos 1; attempt 6 read return #3 as 0.0 — record which return holds the position) and whether track names stick when set via GetSetMediaTrackInfo_String(tr,'NAME',s,#s+1,true) (attempt 6 used buf_len=1 and scratch.rpp showed NAME ""). Append findings under a new '## M6' heading in docs/notes/m5-observations.md: SMTH CUT verdict (40916 works — with measured fade value — or the correct id with measured behavior), marker return order, and the name behavior. Facts from attempt 6: CountMediaItems(nil, track) returns the PROJECT TOTAL in this build (count per-track items by looping GetTrackMediaItem(tr, i) until nil); reaper.GetCommandLabel is nil. Do not modify plugin source, simple_verbs.lua/.env, oracle/fake/simple_verbs.txt, or any Verify; probe_smth.lua is throwaway (may delete it once the note is written).
    Verify: `grep -q '^## M6' docs/notes/m5-observations.md && grep -qi 'smth cut' docs/notes/m5-observations.md`
  - [ ] Finish the parent task using the findings of the step above
    Finish step = make 'tools/reaper-oracle --layer reaper --scenario simple_verbs' exit 0. Read the '## M6' section of docs/notes/m5-observations.md FIRST — it has the SMTH CUT verdict, marker return order, CountMediaItems quirk, and track-name behavior. Then fix oracle/scenarios/simple_verbs.lua: (1) expected cmd lines are 40157, then the SMTH id, then 40346 — attempt 6 expected only two and failed (AUDIO LEVEL double-tap goes through the id table and traces cmd 40157); (2) count items per track by looping GetTrackMediaItem(tr, i) until nil — CountMediaItems(nil, tr) returns the project total; (3) parse marker position with the recorded return order (return #3 was len=0.0, not pos; the marker IS at 1.0); (4) the name-based track lookup failed in attempt 6 — set names per the note's finding, or drop names and read A/B's I_CURTAKE from a pre-duplication sample (script window ~500-1350 ms, before PLACE ON TOP at ~1470) and find B's track by index. If the note says the SMTH id is wrong: change the one line in plugin/src/verbs.cpp (SmthCutTap), rebuild BEFORE rerunning (tools/check — 'tools/reaper-oracle --layer reaper' does NOT rebuild the .so), and update the expected cmd line to the new id. Keep the keep-simple_verbs entry in the default list in tools/reaper-oracle; do not loosen any check; keep the record('smth_cut_max_fadein_auto') measurement.
    Verify: `tools/reaper-oracle --layer reaper --scenario simple_verbs`

### 7. SMOKE-TEST.md: "M6 — takes, marker, view"

- Add the section: E6 (three-take item, CAM 2/3, nothing selected vs. selected), E8 (MARK), SMTH CUT over a jump cut, PLACE ON TOP, VIDEO_ONLY/AUDIO_ONLY re-tap restores, FULL VIEW. List expected results per step (CAM LEDs are M8; note "LED not yet" for CAM).

**Verify**
```bash
grep -q '^## M6 — takes' SMOKE-TEST.md
```

- [ ] Done
