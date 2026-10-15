# Ralph record: 42-release-prep

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- Milestone design docs (`plugin-design-m*`, `plugin-configuration`, `reaper-plugin-workflow`, `reaper-plugin`) live in `docs/archive/` now; so do the vendored `REAPER actions list.htm`, `reaper-api.htm`, `ReaperUserGuide781c/`. The current architecture doc is `docs/design.md`.
- Release version (0.1.0) is single-sourced in `plugin/CMakeLists.txt` `project(VERSION …)`: baked into the binary (`SPEED_REAPER_VERSION`), printed to REAPER's console on load, parsed by `tools/release` for the `dist/speed-reaper-<v>-<platform>.zip` names. `flake.nix`'s `pluginVersion` attr must mirror it (tools/release fails on drift). Nix 2.34's `builtins.match` is anchored (whole-string), which is why the flake does not parse the CMakeLists itself.


## Task: 1. Docs: keep what is still true, archive the rest

### Attempt 1 — 2026-10-15T17:38:40Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No tool or vibes file reads `REAPER actions list.htm`, `reaper-api.htm`, or `ReaperUserGuide781c/` → all three moved to `docs/archive/` too.
- `docs/design.md` written (288 lines ≤ 300) from current code: session.h, wheel_dispatcher.h, bindings.h, verb_registry.h, target.h, led_state.h, shuttle.h, main.cpp, default_bindings.ini; superseded bits (OPTIONS mode, SOURCE focus, ExtState) dropped, "why a text file" argued there.
- All 6 milestone docs + 3 vendored refs moved to `docs/archive/` via git mv; every `docs/<moved>.md` reference in vibes/41, plugin/, oracle/, SMOKE-TEST.md, docs/notes/, docs/*.md repointed to `docs/archive/...` (sed, per-file).
- Verify pattern (`rg 'docs/(plugin-design-m|…)'` excl. archive + vibes/[0-3]*) now matches nothing.
Facts:
- `tools/check` passed (job r42t1check, rc=0, ~18s warm); exact Verify command exits 0. Line 17 ticked; done.
Next:
- (task complete)

Result: exit 0; done; log: logs/42-release-prep-L17-a1-1792085921059.log

### Done — 2026-10-15 — 1 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
```

## Task: 2. Bindings reference generated from the registry

### Attempt 2 — 2026-10-15T17:44:58Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Prior attempt left no partial work (git status clean except this record).
- `MakeDefaultRegistry()` in `plugin/src/verb_registry.cpp` is the registry source (15 verbs); `VerbSpec` has name/gestures/description but no structured args — args syntax lives inside the description (`<id>`, `<move|...> [step=...]`), so the generated args column extracts those tokens.
- Key list: `src/speed_editor/key_map.h` (`KEY_*` constants + `getButtonNames()` labels); `bindings.h` exposes `AllKeys()`/`KeyName()` already linked in PLUGIN_PURE_SOURCES (bindings.cpp).
- Bindings file location: `<resource>/speed-reaper/bindings.ini`, first-run seeded from embedded `plugin/src/default_bindings.ini` (verbatim source for the doc's last section). Actions: reload/open/show/echo/learn (main.cpp g_actions).
- `tools/check` already builds `build/plugin` (CMAKE_CXX_FLAGS=-Werror → tool code must be warning-free); hid_probe's CMake block is the template for linking PLUGIN_PURE_SOURCES + hidapi on Linux.
- Chose the tiny `verb_list` tool over a plugin_host mode: `plugin/tools/verb_list.cpp` (linked against PLUGIN_PURE_SOURCES, `--list-verbs` / `--list-keys`), CMake block guarded `NOT CMAKE_CROSSCOMPILING` (host tool; Windows cross keeps its pure source list, hidapi only on UNIX).
- `tools/gen_bindings_doc [outfile]` (bash, +x required — first tools/check run failed on Permission denied before chmod) assembles docs/bindings.md: static header + verb_list outputs + `cat plugin/src/default_bindings.ini` inside an ini fence. tools/check regenerates into mktemp and `cmp -s` against docs/bindings.md, failing on drift.
- Verified: last ini fence in docs/bindings.md is byte-identical to default_bindings.ini (awk+diff); full Verify `tools/check && test -s docs/bindings.md && grep -q nudge docs/bindings.md` rc=0 (job r42t2verify, ~1 min).
Next:
- (task complete; line 29 ticked)

Result: exit 0; done; log: logs/42-release-prep-L29-a2-1792086298054.log

### Done — 2026-10-15 — 2 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
```

## Task: 3. README

### Attempt 3 — 2026-10-15T17:51:25Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No partial work left (clean tree except this record); README.md did not exist.
- Facts gathered: VID 0x1EDB PID 0xDA0E + udev rule text from SMOKE-TEST.md prereq 3; install paths `~/.config/REAPER/UserPlugins/` / `%APPDATA%\REAPER\UserPlugins\` (SMOKE-TEST.md 5-6, CROSS-PLATFORM.md); bindings at `<resource>/speed-reaper/bindings.ini` seeded first run (main.cpp LoadBindingsText); five actions verbatim from g_actions[] (reload/open/show/echo/learn); LED table from led_state.cpp + key_map.h (SNAP, CUT=ripple1, DIS=ripple2, LIVE_OWR=trim-behind, CAM1-9=active take, VIDEO_ONLY=isolate, AUDIO_ONLY=solo); troubleshooting strings verbatim: `device not found`, `auth failed` (hid_wheel_source.cpp — no "timeout" string; 1 s retry loop), `bind unresolved <name>` (reaper_sinks.cpp).
- No macOS/dylib support anywhere (CMake/flake are Linux + Windows only) — README says macOS not supported yet rather than inventing a .dylib path.
- Licenses: src/speed_editor/LICENSE + docs/speed-editor-examples/{python-driver,cpp-driver}/LICENSE all BSD-3-Clause © 2026 John Howard (port of John-Howard/PythonSpeedEditor, pinned 28057b4 per examples README); no repo-root LICENSE file; vendor/reaper-sdk + vendor/WDL credited under SDK terms.

Result: exit 0; done; 1 file changed, 1 insertion(+), 1 deletion(-); log: logs/42-release-prep-L40-a3-1792086686090.log

### Done — 2026-10-15 — 3 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
 M vibes/42-release-prep.md
```

## Task: 4. License

### Attempt 4 — 2026-10-15T17:54:17Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No partial work left (clean tree except this record).
- Vendored licenses: `src/speed_editor/LICENSE` + `docs/speed-editor-examples/{python-driver,cpp-driver}/LICENSE` all BSD-3-Clause © 2026 John Howard (pinned 28057b4). `vendor/reaper-sdk/sdk/LICENSE` = MIT-flavored grant. `vendor/WDL` = MIT upstream (justinfrankel/WDL) but the vendored copy ships **without** the LICENSE file; headers-only SYSTEM include root (`plugin/CMakeLists.txt:120`), no WDL `.cpp` compiled/linked. `vendor/reaper-sdk/reaper-plugins/` sources unused by the build.
- `docs/speed-editor-examples/python-bmd/` has **no license** (all rights reserved, per its own README table) — flagged in the note as reference-only, not ported into shipped code, explicitly excluded from the root license; no permissive license can cover it, but it doesn't block the choice since every *licensed* vendored file is permissive.
- Chose **BSD-3-Clause** root (identical to the largest vendored component; © line = repo author Vincent Khougaz per `git log`); LICENSE footer lists the vendored components + pointer to the note.
- `docs/notes/licensing.md` written with the per-component compatibility table (6 rows) + why-BSD-3 rationale.
- Verify `test -s LICENSE && test -s docs/notes/licensing.md` exits 0.
Next:
- (task complete; line 51 ticked)

Result: exit 0; done; log: logs/42-release-prep-L51-a4-1792086857430.log

### Done — 2026-10-15 — 4 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
```

## Task: 5. Test suite: full and fast

### Attempt 5 — 2026-10-15T17:57:17Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No partial work left (clean tree except this record). Build dirs warm.
- REAPER_SCENARIOS (tools/reaper-oracle, 20 scenarios: load…xfade_keys) — every one has an oracle/scenarios/<n>.lua. Non-suite lua files: led_reads, noop, spike_xfade — all three already have top comments explaining why kept (M8 research probe / crash-capture + timing baseline / M9 record-only spike); I add an explicit "not in the suite" line to each.
- "User bindings path" = LoadBindingsText() in main.cpp (user's <resource>/speed-reaper/bindings.ini; missing → seed + embedded default). It calls REAPER API so it's not linkable into the pure unit tests → extract the read-with-fallback into a pure LoadUserBindings(path, fallback, seed_if_missing) in bindings.cpp, main.cpp keeps seeding (dir create + write + trace) via callback.
- Full `ORACLE_FULL=1 tools/check` (pre-change, warm) = **1m54s**, ORACLE PASS all 20 scenarios (job r42t5check). So <3 min is in sight; the no-args `tools/check` additionally runs `tools/windows-oracle --layer static` (MinGW cross build).
- Extracted `LoadUserBindings(path, fallback, seed_if_missing)` into bindings.cpp (pure, in PLUGIN_PURE_SOURCES); main.cpp LoadBindingsText now calls it, seed callback does RecursiveCreateDirectory + write + Trace. New test plugin/test/user_bindings_test.cpp: missing → default + seed once; empty file → "" (no fallback, no seed) + ParseBindings → 0 bindings/0 errors; malformed file → verbatim text, 3 error lines, 1 good binding survives. Test passes standalone (rc=0).
- Caveat: bare shell has no cmake/reaper — must prefix `nix develop --command`; tools/check self re-execs into the dev shell.
- Job r42t5verify results (post-change): `ORACLE_FULL=1 tools/check` = **1m50s**, ORACLE PASS all 20 scenarios, ctest 20/20 (incl. new user_bindings_test). No-args `tools/check` = **20s** warm (< 3 min) → no `docs/notes/check-timing.md` needed (task says only "if not"). Cold caveats: the MinGW cross build (`tools/windows-oracle --layer static`) and a full recompile dominate on a cold nix store; not a problem here.
Next:
- (task complete; line 64 ticked)

Result: exit 0; done; log: logs/42-release-prep-L64-a5-1792087037999.log

### Done — 2026-10-15 — 5 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
```

## Task: 6. Cross-platform release builds

### Attempt 6 — 2026-10-15T18:07:24Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No partial work from earlier attempts (tree clean except this record). No `tools/release`, no `dist/`, no `.rules` file in repo, `plugin/CMakeLists.txt` `project()` had no VERSION.
- Base shell: no cmake (tools/check self-reexecs into `nix develop`), nix 2.34.6 + python3 present, no `zip`/`7z` → zips via python3 `zipfile` (python3 is in the devShell too).
- CROSS-PLATFORM.md has no macOS path → README says "unsupported, build from source".
- Console-on-load: `ShowConsoleMsg` with `"!SHOW:"` prefix appends to REAPER console without opening the window (SDK comment, reaper_api.h already WANTs it; main.cpp onTimer uses it).
- Plan: version 0.1.0 from `project(... VERSION ...)`; CMake `add_compile_definitions(SPEED_REAPER_VERSION=...)`; flake parses the same CMakeLists for the plugin derivation version (single source); new `share/99-blackmagic-speed-editor.rules` (content = README inline rule); `tools/release` builds Linux .so in `build/release-linux` (production, FAKE_DEVICE OFF) + Windows via `nix build .#reaper-plugin-windows`, zips `dist/speed-reaper-<v>-{linux,windows}-x86_64.zip` (binary + README.md + LICENSE + docs/bindings.md + udev rule for linux).
- **Nix 2.34 `builtins.match` is ANCHORED** (pattern must match the whole string; `.*` does not cross newlines); returns `[]` for a groupless full match, `null` for no match, capture list otherwise. First `nix build .#reaper-plugin-windows` died with "expected a list but found null" because my unanchored whole-file pattern silently no-matched → pluginVersion null. Shell-quoting trap: nix code via `--expr` must use SINGLE quotes or the shell eats `\(` escapes (caused bogus "invalid regular expression" debug results); `nix-instantiate --eval -f` and piped `nix repl` are unusable here.
- Decision: dropped flake-side CMakeLists parsing (too fragile under anchored match); flake keeps `pluginVersion = "0.1.0"` with a sync note, and `tools/release` fails if `nix eval .#packages.x86_64-pc-windows-gnu.reaper-plugin.version` != the CMakeLists version.
- First verify run (job 42t6verify): everything passed up to `tools/windows-oracle --layer static`, which failed rc=1 silently — root cause = the flake eval failure above (oracle does `nix build ... 2>/dev/null` → empty path, then exits 1).
- ReaPack index.xml: skipped, documented with why in docs/notes/reapack.md.
- `zipfile` rejects nix-store files (epoch-0 mtime, "ZIP does not support timestamps before 1980") → tools/release writes zips with a fixed 1980-01-01 timestamp (also reproducible zips) and explicit arcnames so the binary sits at the zip root (`reaper_speed_reaper.so` / `.dll`, not build/ or /nix/store paths).
- Debug detour: the Linux .so appeared to lack the console banner — false alarm. GCC 16 stores >15-char string literals chunked (16+4 bytes, memcpy-chunked std::string ctor), so `strings` only shows `!SHOW:Speed Reap`. Runtime printf test of the exact expression prints `!SHOW:Speed Reaper v0.1.0 loaded` correctly. (Note: a `const char* f(){ return (std::string(…) + …).c_str(); }` test shape is UB — the temp dies; the plugin's inline argument usage is safe.)
- Final state: `tools/check && tools/release && ls dist/*.zip` = rc 0 (jobs 42t6verify3, 42t6final). dist/ has speed-reaper-0.1.0-linux-x86_64.zip (reaper_speed_reaper.so, README.md, LICENSE, docs/bindings.md, 99-blackmagic-speed-editor.rules) and speed-reaper-0.1.0-windows-x86_64.zip (dll + same docs, no udev rule). Version single-sourced in plugin/CMakeLists.txt; tools/release hard-fails if the flake's pluginVersion attr drifts.
- New files: tools/release (executable), share/99-blackmagic-speed-editor.rules, docs/notes/reapack.md. Linux prod .so builds in build/release-linux (FAKE_DEVICE OFF; build/plugin stays the FAKE_DEVICE=ON test build). SKIP_WINDOWS=1 supported in tools/release.
Next:
- (task complete; line 77 ticked)

Result: exit 0; done; log: logs/42-release-prep-L77-a6-1792087644123.log

### Done — 2026-10-15 — 6 attempt(s)
Files:
```
M vibes/.ralph/42-release-prep.md
```

## Task: 7. SMOKE-TEST.md becomes one release checklist

### Attempt 7 — 2026-10-15T18:24:07Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- No partial work from earlier attempts (tree clean except this record); SMOKE-TEST.md still had the per-milestone layout (556 lines).
- Rewrote SMOKE-TEST.md as one `## Release checklist` (12 ordered steps: clean-profile install from the zip → device found → wheel modes + LEDs → transport → split/trim/ripple → holds → takes → view → crossfade keys → bindings actions (show/echo/edit+reload/learn/restart persistence/delete→default) → unplug/replug → REAPER restart), each with Action / Expected / `- [ ] pass`; speedcli Tests 1–4 kept verbatim under `## Appendix`.
- Facts pulled from the real sources, not memory: zip contents (binary at zip root + README/LICENSE/docs/bindings.md, linux zip adds `99-blackmagic-speed-editor.rules`), default binding groups + action ids from `plugin/src/default_bindings.ini` (SPLIT=40012, IN/OUT=40625/40626, clear=40635, TRIM 40511/40512, deselect-all 40289, undo 40029, marker 40157, volume readout, fullscreen 40346, crossfades TRANS=41059 / DIS=40916 / CUT=41193 / TRANS_DUR=43776), plugin actions + echo name from docs/bindings.md.
- Clean-profile step: `reaper --dir /tmp/reaper-smoke` (Linux) or portable mode / move `%APPDATA%\REAPER` (Windows) — not verified against a live REAPER install, phrased with a fallback.
- Verify command exits 0; no `^## M[0-9]` headings remain (milestone content folded into the checklist, milestone cross-references dropped).
Next:
- (task complete; line 88 ticked)

Result: exit 0; done; 3 files changed, 288 insertions(+), 473 deletions(-); log: logs/42-release-prep-L88-a7-1792088647884.log

### Done — 2026-10-15 — 7 attempt(s)
Files:
```
M SMOKE-TEST.md
 M vibes/.ralph/42-release-prep.md
 M vibes/42-release-prep.md
```

## Task: 8. Human checkpoint: hand test and tag

### Attempt 8 — 2026-10-15T18:27:40Z — budget 1800s
<!-- worker: add Facts / Dead ends / Next bullets below while you work -->
Facts:
- Task 8 is a human checkpoint: it requires a human to run `SMOKE-TEST.md` "Release checklist" on the physical device, record failures, and then tag `v0.1.0`. None of that is doable by a worker (no device, no REAPER on this box, and tagging without the human's sign-off would defeat the checkpoint). Marked `- [!]` BLOCKED — this is the intended outcome for a human gate, not a failure.
- Verified all prerequisites are in place: `dist/speed-reaper-0.1.0-linux-x86_64.zip` (380 KB) and `dist/speed-reaper-0.1.0-windows-x86_64.zip` (4.3 MB) exist and are current (rebuilt by task 6); `## Release checklist` present at SMOKE-TEST.md:26 (12 ordered steps, ~20 min); version 0.1.0 single-sourced at `plugin/CMakeLists.txt:27`; **no git tag exists yet** (`git tag -l` empty) — the human should create `v0.1.0` after the checklist passes.
- Optional squash before tagging: per-milestone commits exist (tasks 5–6 each have 2 commits, e.g. 106ee3f + f2fe0b4); the human may `git rebase -i` them if desired, but the tags/history are not yet cut so it's safe.
Next:
- (human action: run SMOKE-TEST.md Release checklist on the device, file failures as new tasks under section 8, then `git tag v0.1.0`; when done, tick the box — no verify command exists for this task)

Result: exit 0; worker-blocked; 2 files changed, 13 insertions(+), 1 deletion(-); log: logs/42-release-prep-L94-a8-1792088860749.log

### Replan (depth 0) — BLOCK → Human checkpoint for v0.1.0: run the SMOKE-TEST.md 'Release checklist' on the device using dist/speed-reaper-0.1.0-linux-x86_64.zip (and the windows zip if available); record any failures as new tasks under section 8, and if it passes, optionally squash the per-task duplicate commits for tasks 5–6, then run `git tag v0.1.0` and tick the section 8 box. If you hit failures, add them as tasks under section 8 instead — ralph will resume from there.
Trigger: worker marked it blocked: Human gate — requires the human to run the SMOKE-TEST.md "Release checklist" on the physical device with `dist/speed-reaper-0.1.0-{linux,windows}-x86_64.zip`, record any failures as new tasks here, and then run `git tag v0.1.0` (no tag exists yet). A worker cannot hand-test or sign off the release; all prerequisites are verified in place (see vibes/.ralph/42-release-prep.md, Attempt 8).
Why: Task 8 is a genuine human gate: the SMOKE-TEST.md Release checklist must be run on the physical device and v0.1.0 tagged with human sign-off; attempt 8 verified all prerequisites (both dist zips current, checklist at SMOKE-TEST.md:26, no tag yet), so the worker correctly stopped instead of guessing.
Rejected: SPLIT: no worker-doable subtasks remain — every prerequisite (zips, checklist, version) was already verified in place; SPLIT/SPIKE would invent work that does not advance the gate. REWRITE: the task text is clear and its missing Verify is intentional for a human checkpoint. BUDGET: attempt 8 finished in-budget (exit 0); time or model would not let a worker hand-test or sign off.
Log: logs/42-release-prep-L94-replan-1792088933353.log
