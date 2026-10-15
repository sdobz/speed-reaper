# 42 — Release prep: documentation, testing, packaging

Builds on `41-action-registry.md`. Goal: a vanilla REAPER extension someone else can install, understand from the bindings file + README, and trust. Every Verify starts with `tools/check`.

### 1. Docs: keep what is still true, archive the rest

- Create `docs/design.md` (≤ 300 lines): the architecture as it is now — device worker → `Session` → `GestureEngine` → bindings → verbs; `WheelDispatcher`; targeting rule (`WithTargetSelected`, nearest item); LED status bar meaning; shuttle law; held-verb mechanism; why config is a text file. Pull facts from `plugin-design-m*.md`, `reaper-plugin-workflow.md`, `plugin-configuration.md`; drop everything superseded (OPTIONS mode, SOURCE focus, ExtState bindings).
- Move to `docs/archive/`: `plugin-design-m1-m3.md`, `plugin-design-m4-shuttle-impl.md`, `plugin-design-m5-m9.md`, `plugin-configuration.md`, `reaper-plugin-workflow.md`, `reaper-plugin.md`. Keep in place: `speed-editor-capabilities.md`, `speed-editor-workflow.md`, `reaper-workflow.md`, `reaper-api-subset.md`, `reaper-actions.tsv`, `plugin-oracle-design.md`, `windows-oracle-design.md`, `reaper-dev-env.md`, `docs/notes/`.
- Large vendored references (`REAPER actions list.htm`, `reaper-api.htm`, `ReaperUserGuide781c/`): keep only if `rg` finds a tool or vibes file reading them; otherwise move to `docs/archive/` too.
- Fix every link that pointed at a moved file (`rg -l` over `docs/ vibes/4* plugin/ tools/ SMOKE-TEST.md CROSS-PLATFORM.md`).

**Verify**
```bash
tools/check && test -s docs/design.md && ! rg -n 'docs/(plugin-design-m|plugin-configuration|reaper-plugin-workflow|reaper-plugin\.md)' --glob '!docs/archive/**' --glob '!vibes/[0-3]*' .
```

- [x] Done

### 2. Bindings reference generated from the registry

- Add a `--list-verbs` mode to `plugin/tools/plugin_host.cpp` (or a tiny `verb_list` tool linked against the pure sources) printing every registered verb: name, accepted gestures, args, description, as Markdown.
- Generate `docs/bindings.md`: header (file location, syntax, gestures, `step=`, actions reload/open/show/echo/learn), the key-name list from `key_map.h` with the printed labels, the verb table (generated), and the default file verbatim. A `tools/check` step regenerates it and fails if the committed copy differs.

**Verify**
```bash
tools/check && test -s docs/bindings.md && grep -q 'nudge' docs/bindings.md
```

- [x] Done

### 3. README

- Rewrite/create `README.md` (≤ 150 lines): what it is (Speed Editor → REAPER, tap/double/hold+turn), install per platform (where the `.so`/`.dll`/`.dylib` goes, udev rule on Linux, Resolve must not hold the device), first run (bindings file written, where), the five actions, a "make it yours" section (edit → reload, learn, echo to discover), LED meanings table, troubleshooting (device not found, auth timeout, unresolved binding), build from source (`nix build`, `tools/check`), license and credits (vendored driver/examples: check each `LICENSE` in `src/speed_editor/` and `docs/speed-editor-examples/`).

**Verify**
```bash
tools/check && test -s README.md && grep -q 'bindings.ini' README.md && grep -qi 'udev' README.md
```

- [x] Done

### 4. License

- Add `LICENSE` at the repo root. Choose one compatible with every vendored license (`src/speed_editor/LICENSE`, `vendor/WDL`, `vendor/reaper-sdk`, `docs/speed-editor-examples/*/LICENSE`); write the compatibility check as a short table in `docs/notes/licensing.md`. If a vendored file is incompatible with any permissive choice, stop and say so in the note instead of picking.

**Verify**
```bash
test -s LICENSE && test -s docs/notes/licensing.md
```

- [ ] Done

### 5. Test suite: full and fast

- `ORACLE_FULL=1 tools/check` passes. Every scenario in `REAPER_SCENARIOS` exists, every `oracle/scenarios/*.lua` not in the suite is either a documented probe (comment at its top saying why it is kept) or deleted.
- `tools/check` without args finishes in < 3 min on this machine; if not, list the slow steps in `docs/notes/check-timing.md` and move the slowest optional step behind an env flag.
- Add a test that loads the **user** bindings path code with a missing, empty, and malformed file (falls back / skips lines, never crashes).

**Verify**
```bash
ORACLE_FULL=1 tools/check
```

- [ ] Done

### 6. Cross-platform release builds

- Follow `CROSS-PLATFORM.md`: Linux `.so` (nix), Windows `.dll` (MinGW cross, `tools/windows-oracle`), macOS only if the doc gives a working path (otherwise state "unsupported, build from source" in README).
- `nix build .#release` (or a `tools/release` script) produces `dist/speed-reaper-<version>-<platform>.zip` each containing the binary, `README.md`, `LICENSE`, `docs/bindings.md`, and the udev rule for Linux. Version comes from one place (`plugin/CMakeLists.txt` `project(VERSION …)`), also printed to the console on load.
- Optional: a ReaPack `index.xml` pointing at the release zips (`docs/notes/reapack.md` if skipped, with why).

**Verify**
```bash
tools/check && tools/release && ls dist/*.zip
```

- [ ] Done

### 7. SMOKE-TEST.md becomes one release checklist

- Replace the per-milestone sections (M1…M8, Bindings) with one ordered checklist a human runs on real hardware in ≤ 20 minutes: install from the release zip into a clean REAPER profile → device found → wheel modes + LEDs → each default binding group (transport, split/trim/ripple, holds, takes, view, crossfade keys) → bindings actions (show, echo, edit+reload, learn, restart persistence, delete file → default) → unplug/replug → REAPER restart. Each step: action, expected result, checkbox. Keep the speedcli device tests (Tests 1–4) as an appendix.

**Verify**
```bash
grep -q '^## Release checklist' SMOKE-TEST.md && ! grep -q '^## M[0-9]' SMOKE-TEST.md
```

- [ ] Done

### 8. Human checkpoint: hand test and tag

- The human runs `SMOKE-TEST.md` "Release checklist" on the device with the Linux zip (and Windows if available), records failures as new tasks below this one, then tags `v<version>`. Optionally squash the per-task duplicate commits per milestone before tagging.

- [ ] Done
