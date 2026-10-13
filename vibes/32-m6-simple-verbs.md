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

### 6. Oracle scenario: `simple_verbs`

- Add `oracle/fake/simple_verbs.txt` + `.env` + `simple_verbs.lua`. Setup: track 1 "A" with item A [0,5] holding two takes, track 2 "B" with item B [0,5] one take, all takes with tone PCM sources, nothing selected, cursor 1; after PLACE ON TOP the script resizes to the real SMTH scenario — A [0,1.5], C (A's copy) [1.5,5] meeting at 1.5, strictly inside tsel [1,2] (the proven 40916 pattern; the .lua is authoritative). Assertions: CAM2 → A's `I_CURTAKE`==1, B's 0; AUDIO LEVEL double-tap → marker count +1 at cursor; time selection [1,2] on the meeting A/C boundary + SMTH CUT → `GetMediaItemInfo_Value(C,"D_FADEINLEN_AUTO")` or the crossfade the build produces is >0 (record what you measure); PLACE ON TOP → track count +1 and item count +1; AUDIO_ONLY twice → solo flag set then cleared; VIDEO_ONLY twice → track 2 hidden then shown; FULL VIEW → trace `cmd 40346` and the command exists (`GetToggleCommandState` not -1). Fix wrong ids in the tables and record them in `docs/notes/m5-observations.md`; do not loosen checks. Add to the default oracle run.

**Verify**
```bash
tools/reaper-oracle --layer reaper --scenario simple_verbs
```

- [ ] Done
  <!-- ralph: replanned=1 op=AND -->
  - [x] Spike: settle the SMTH CUT command (does 40916 crossfade? fallback candidate `41059`) — findings to docs/notes/m5-observations.md
    <!-- ralph: replanned=1 -->
    oracle/scenarios/probe_smth.lua is ALREADY WRITTEN (configs A–F: 40916 and 41059 on meeting/overlapping items on one or two tracks; plus marker and track-name probes) — do not rewrite it; only fix the exact line if a run reports a Lua error in the output.
    1. Run: `tools/reaper-oracle --layer reaper --scenario probe_smth` (it re-execs into the nix dev shell itself).
    2. Read `build/oracle/probe_probe_smth.json` → `info`: keys `A_before/A_after_40916`, `B_…`, `E_…` (40916), `C_…/D_…/F_…` (41059), `marker_count`, `marker_ret1` (the 8 EnumProjectMarkers returns, |-joined in order), `name_set_len1`, `name_read_len1`, `name_setB_len1`.
    3. Append to `docs/notes/m5-observations.md` under a new `## M6` heading, exactly three bullets: `- SMTH CUT: <id> works — config <X>, measured finA/foutA/xf = <values>` (any non-zero fade = works; if all six configs show all-zero fades: `- SMTH CUT: no id works — 40916 and 41059 both no-op in configs A–F, all fades 0`); `- Marker: EnumProjectMarkers return #<n> is the position (marker at 1.0) — order: <marker_ret1 values>`; `- Track name: <sticks|does not stick> — which set/read calls and buf_len work, with the read-back values`.
    4. Facts: `CountMediaItems(nil,tr)` returns the PROJECT total; this Lua build lacks `reaper.Main_NewProject`, `GetSetEditCurPos`, `GetCommandLabel`; do not reset the project.
    Do not touch simple_verbs.lua/.env, oracle/fake/, plugin source, or any Verify; probe_smth.lua is throwaway (may delete it after the note is written).
    Verify: `grep -q '^## M6' docs/notes/m5-observations.md && grep -qi 'smth cut' docs/notes/m5-observations.md && grep -qi 'enumprojectmarkers' docs/notes/m5-observations.md && grep -qi 'track name' docs/notes/m5-observations.md`
  - [!] Make simple_verbs pass: put a meeting A/C boundary at 1.5 (inside tsel [1,2]) with tone sources — the proven 40916 pattern
    <!-- ralph: replanned=1 -->
    Facts (docs/notes/m5-observations.md ## M6 CORRECTION + build/oracle/probe_probe_sv4.json, do NOT re-probe, do NOT touch plugin source): 40916 sets auto-fades only at an item boundary strictly inside the tsel where an item ENDS — meeting [1,1.5]+[1.5,2] tsel[1,2] gave D_FADEOUTLEN_AUTO=0.99 / D_FADEINLEN_AUTO=0.99, meeting [0,2.5]+[2.5,5] tsel[1,4] gave 3.0; spans without an internal end-boundary no-op (probe_sv5). All working probes used tone PCM sources; the empty-take spike no-oped even with a good boundary.
    1. In oracle/scenarios/simple_verbs.lua, in the existing st 1500-1850 window (tsel [1,2] is set there; it runs after PLACE ON TOP ~1440 ms and before SMTH CUT ~1910 ms): C's track = the track whose P_NAME is neither 'A' nor 'B' (index 0 after duplication), A's track = name 'A'. Set A's item D_LENGTH=1.5 and C's first item D_POSITION=1.5 → A [0,1.5] meets C [1.5,5] at 1.5, strictly inside tsel [1,2].
    2. Tone sources: copy le16/le32/write_tone from oracle/scenarios/probe_sv4.lua (write a WAV under a distinct name in GetResourcePath()), create the PCM source once, set it on A's takes 0 AND 1 (CAM2 switches I_CURTAKE to 1) and B's take at setup, and on C's takes in the 1500-1850 window in case duplication drops sources (SetMediaItemTake_Source on GetMediaItemTake(it, n)).
    3. Update the stale setup sentence in this block of vibes/32-m6-simple-verbs.md ('Setup: track 1 with item A [0,5] ... overlapping A/C') to the real scenario: A [0,1.5], C [1.5,5] meeting at 1.5 inside tsel [1,2], tone sources; the .lua is authoritative.
    4. Keep every check as-is (the SMTH assertion already accepts any crossfade field >0 via maxauto; if the fade lands on A's D_FADEOUTLEN_AUTO instead of C's fadein that is fine — record('smth_cut_max_fadein_auto') / record('smth_cut_max_fadeinlen_auto') stay and record what was measured). Do not touch oracle/fake/, other scenarios, tools/reaper-oracle, or any Verify.
    Verify: `tools/reaper-oracle --layer reaper --scenario simple_verbs`
    BLOCKED: still failing after a replan (3 attempt(s), last: exit 0, progress); see vibes/.ralph/32-m6-simple-verbs.md

### 7. SMOKE-TEST.md: "M6 — takes, marker, view"

- Add the section: E6 (three-take item, CAM 2/3, nothing selected vs. selected), E8 (MARK), SMTH CUT over a jump cut, PLACE ON TOP, VIDEO_ONLY/AUDIO_ONLY re-tap restores, FULL VIEW (`40346` is REAPER's "Toggle fullscreen", not "maximize arrange"; no such action exists in `docs/reaper-actions.tsv` — record what it actually does). List expected results per step (CAM LEDs are M8; note "LED not yet" for CAM).

**Verify**
```bash
grep -q '^## M6 — takes' SMOKE-TEST.md
```

- [ ] Done
