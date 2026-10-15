# 41 — Verb registry + bindings file

> Command ids: look up by number or name in `docs/reaper-actions.tsv` (columns `section	id	name`). Never guess an id.

Builds on `40-cleanup.md`. Replaces `36-m10-bindings.md` (ExtState binding table for the "non-obvious" keys only) and the crossfade decision of `35-m9-crossfade-spike.md`. Dropped on purpose (from `30-backlog.md` / `docs/plugin-configuration.md`): the on-device OPTIONS mode, the obvious/non-obvious split, SOURCE focus, composite-gesture authoring.

**Goal:** the user shapes the device to their workflow, and their workflow to the device. Every key is bindable; the shipped default file *is* the manual. Fancy behaviors (place on top, take switching, …) stay available as named verbs but are just lines in the file.

## Design (normative for this file)

Three layers: **input** `(KEY, gesture)` → **binding** (one line) → **verb** (named, registered in C++).

File: `<GetResourcePath()>/speed-reaper/bindings.ini`. Written from a compiled-in default on first run if missing; never overwritten afterwards.

```ini
[tuning]
double_tap_ms = 300
hold_ms = 350

[bindings]
; KEY.gesture = verb [args]        ; comment
SPLIT.tap        = action 40012              ; Item: Split items at edit or play cursor
SPLIT.turn       = nudge move                ; hold SPLIT + wheel: move target items
TRIM_IN.tap      = targeted 40511            ; trim left edge to cursor, on the cursor item
TRIM_IN.turn     = nudge trim_in step=frame  ; hold + wheel: drag the left edge
ESC.double       = action 40029              ; Edit: Undo
JOG.press        = wheel jog
STOP_PLAY.press  = transport play_stop
```

- **KEY** = the `KEY_*` suffix from `src/speed_editor/key_map.h` (`SPLIT`, `TRIM_IN`, `CAM1`, …).
- **gesture**: `press` (fires on key-down, no tap wait — wheel modes, transport), `tap`, `double`, `turn` (hold + wheel; the verb gets begin / step / end). A key bound with `press` must not have other gestures (parse error for that key).
- A key waits for a second tap **only if it has a `.double` binding** (the double-tap set is derived, not listed).
- **Verbs:** `action <id|_NamedCmd>` (numeric → `Main_OnCommand`, else `NamedCommandLookup`; unresolved → trace `bind unresolved <cmd>`, no-op); `targeted <id|_Named>` (same inside `WithTargetSelected` + one undo block); plus named built-ins (wheel, transport, nudge, take, …). Each verb declares the gestures it accepts and a one-line description.
- **`turn` verbs** share one held-verb mechanism: targets resolved once at hold start, selection snapshot, one undo block, restored/closed at release; no targets → trace `target none`, no undo block. Arg `step=frame|grid|sample|<N>ms` (default `frame`), ticks-per-step from `[tuning] hold_ticks_per_step` (default 4).
- Errors never stop loading: bad line → trace + console line `bindings.ini:<n>: <reason>`, line skipped.

Every Verify starts with `tools/check`.

### 1. `VerbRegistry` (pure)

- `plugin/src/verb_registry.h/.cpp`: `struct VerbContext { IActionSink&; IProject&; }`; `struct VerbSpec { name; gestures accepted; description; factory(args) -> std::unique_ptr<Verb> or error }`; `class Verb { virtual void Run(VerbContext&) {}  virtual void Begin(VerbContext&) {}  virtual void Step(VerbContext&, int delta) {}  virtual void End(VerbContext&) {} }`. Per-verb state (e.g. the TIMELINE select accumulator, today `VerbRouter::select_delta_`) lives in the verb instance.
- Register every behavior that exists today, logic moved verbatim from `verbs.cpp`/`router.cpp`: `action`, `targeted`, `ripple_delete`, `select_cursor_item` (TIMELINE tap), `select_step` (TIMELINE turn), `take <N>`, `volume_readout`, `smooth_cut`, `place_on_top`, `isolate_tracks`, `solo_tracks`, `nudge <move|trim_in|trim_out|slip_src|slip_dest|volume>`, `wheel <jog|scrl|shtl>`, `transport play_stop` (incl. the SHTL hard-stop + return-to-prior-mode rule).
- `verb_registry_test.cpp`: every name resolves; wrong gesture for a verb is rejected; bad args (`take 0`, `nudge foo`) rejected with a message.
- Do not wire it in yet.

**Verify**
```bash
tools/check verb_registry
```

- [ ] Done

### 2. Bindings parser (pure)

- `plugin/src/bindings.h/.cpp`: `ParseBindings(std::string_view text) -> {Tuning, std::map<(key, gesture), BoundVerb>, std::vector<std::string> errors}` using the registry. INI as above: `;`/`#` comments, blank lines, whitespace-tolerant, unknown section → error line.
- `bindings_test.cpp`: numeric id, `_RSabc`, `targeted`, unknown key, unknown gesture, unknown verb, `press` + `tap` on one key, tuning values + defaults, derived double-tap set, errors carry line numbers.

**Verify**
```bash
tools/check bindings
```

- [ ] Done

### 3. Default bindings = today's behavior

- `plugin/src/default_bindings.ini`, embedded at build time (CMake `file(READ)` → generated header). It reproduces exactly the current `kVerbTable`, `CompositeRows`, `kHoldRows`, `VerbDoubleTapKeys`, and `Router` key rules — one commented line each, comment = what it does in REAPER terms.
- Golden test `default_bindings_test.cpp`: parses with zero errors; derived double-tap set equals today's `{IN, OUT, ESC, TIMELINE, AUDIO_LEVEL}`; every (key, gesture) today's tables map is bound to the matching verb.

**Verify**
```bash
tools/check default_bindings
```

- [ ] Done

### 4. Switch over and delete the old tables

- `Session` builds a `BindingTable` from the file (or the embedded default if missing/unreadable) and routes **every** key through `GestureEngine` → table → verb. `press` bindings fire on the down edge (gesture engine emits `Press` for keys whose only binding is `press`). Delete `kVerbTable`, `CompositeRows`, `kHoldRows`, `VerbDoubleTapKeys`, `Router::OnButton` key handling, and the key `switch` in the session. `Router` keeps only wheel mode state + shuttle law (rename if it is now just that).
- First run writes `<resource>/speed-reaper/bindings.ini` from the default. Add `REAPERAPI_WANT_GetResourcePath`, `NamedCommandLookup`.
- Split `verbs_test.cpp`: verb behavior tests move to `verb_registry_test.cpp` (they construct verbs directly); routing tests run through the default bindings. Assertions unchanged.

**Verify**
```bash
tools/check && ! rg -q 'kVerbTable|CompositeRows|kHoldRows|VerbDoubleTapKeys' plugin && tools/reaper-oracle --layer reaper
```

- [ ] Done

### 5. Actions: reload, open, show, echo

Register (search "Speed Reaper" in the action list):
- `speed_reaper_reload_bindings` — re-read the file; console: `Speed Reaper: bindings loaded n=<count>` + each error line.
- `speed_reaper_open_bindings` — open the file in the OS default editor (create from default first if missing).
- `speed_reaper_show_bindings` — print the live table to the console, grouped by key: `TRIM_IN  tap   targeted 40511   Trim left edge…`; then unbound keys.
- `speed_reaper_echo` (toggle action, reports its state) — while on, every fired binding prints `KEY.gesture → verb args`; also traced.
- Oracle scenario `bindings`: Lua writes a bindings file binding `SYNC_BIN.tap = action 40157` and `CLOSE_UP.tap = targeted 40006`, runs reload, fake presses SYNC_BIN (marker +1), CLOSE_UP with an unselected item under the cursor (item removed, selection unchanged), APPND (unbound → no `cmd` trace); a bad line produces the `bindings.ini:<n>:` trace. Add to `REAPER_SCENARIOS`. Read `docs/notes/oracle-scenario-rules.md` first.

**Verify**
```bash
tools/check && tools/reaper-oracle --layer reaper --scenario bindings
```

- [ ] Done

### 6. Learn action

- `speed_reaper_learn` — `PromptForAction` session (open, poll from the timer, close) to pick an action; console prompts "press a device key + gesture (tap / double / hold+turn)"; the next gesture is written as `KEY.gesture = action <id or named>` into the file (replace an existing line for that key+gesture in place, else append to `[bindings]`), then reload. ESC on the device during learn cancels (ESC itself is not learnable during learn). 10 s timeout cancels.
- Pure part (line rewrite in the INI text, preserving comments) unit-tested in `bindings_test.cpp`. `PromptForAction` is not headless: add SMOKE-TEST.md "Bindings" section with the hand checklist (learn → fire → restart REAPER → still bound; edit file + reload; delete file → default rewritten).

**Verify**
```bash
tools/check bindings && grep -q '^## Bindings' SMOKE-TEST.md
```

- [ ] Done

### 7. More `turn` handles + `step=`

- Implement `step=` (frame / grid via snap settings / sample / `<N>ms`) for all `nudge` verbs; default unchanged (`frame`).
- New nudge targets: `fade_in`, `fade_out` (D_FADEINLEN/D_FADEOUTLEN, clamped ≥ 0 and ≤ item length), `roll` (the edit point between the cursor item and its right neighbor on the same track: both edges move, total length fixed; no neighbor → `target none`), `sel_in`, `sel_out` (time selection edges; none → no-op).
- Unit tests in `nudge_test.cpp`; one oracle scenario `hold_handles` (fake holds a key bound to each new verb, asserts geometry). Default bindings: `IN.turn = nudge sel_in`, `OUT.turn = nudge sel_out`, `ROLL.turn = nudge roll`, others commented-out suggestions.

**Verify**
```bash
tools/check nudge && tools/reaper-oracle --layer reaper --scenario hold_handles
```

- [ ] Done

### 8. Crossfade keys as plain bindings

- Run the committed `spike_xfade.lua` (`tools/reaper-oracle --layer reaper --scenario spike_xfade --report-only`) and write `docs/notes/crossfade-actions.md`: table of what each candidate id does to fade lengths (`41059` crossfade overlapping items, `43776` set crossfade length to default, `41191`–`41193` remove fades, `40916` crossfade in time selection, `41118`/`41119` auto-crossfade on/off).
- Pick by rule: bind a key only to an id whose recorded effect matches its label (TRANS/DIS → creates a crossfade at the cursor edit; CUT → removes it; TRANS_DUR.tap → default length). Labels with no matching id stay unbound with a commented suggestion. `TRANS_DUR.turn = nudge fade_in` style binding only if the note shows it reads as "crossfade length".
- Extend the oracle scenario `bindings` (or add `xfade_keys`) with the chosen defaults.

**Verify**
```bash
tools/check default_bindings && test -s docs/notes/crossfade-actions.md && tools/reaper-oracle --layer reaper
```

- [ ] Done

### 9. Simplify the defaults

- Review `default_bindings.ini` against the printed labels: built-in composites stay bound only where the label clearly says what they do (PLACE ON TOP, CAM 1–9, RIPL DEL, TRIM IN/OUT). Labels with no clear REAPER meaning (SMART_INSRT, APPND, RIPL_OWR, SRC_OWR, CLOSE_UP, SYNC_BIN, LIVE_OWR, SOURCE) ship **unbound**, each with 1–2 commented suggestions using plain action ids from `docs/reaper-actions.tsv`.
- Update `default_bindings_test.cpp` to the new expected set; update any oracle scenario whose key became unbound by binding it in the scenario's own bindings file.

**Verify**
```bash
tools/check && tools/reaper-oracle --layer reaper
```

- [ ] Done
