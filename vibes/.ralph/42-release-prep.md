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
