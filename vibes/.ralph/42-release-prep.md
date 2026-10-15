# Ralph record: 42-release-prep

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- Milestone design docs (`plugin-design-m*`, `plugin-configuration`, `reaper-plugin-workflow`, `reaper-plugin`) live in `docs/archive/` now; so do the vendored `REAPER actions list.htm`, `reaper-api.htm`, `ReaperUserGuide781c/`. The current architecture doc is `docs/design.md`.


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
