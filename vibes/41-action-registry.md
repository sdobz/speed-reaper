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

- [x] Done
  <!-- ralph: replanned=1 op=AND -->
  - [x] Registry core + 10 tap verbs + basic tests
    Read plugin/src/verbs.cpp (free functions to move verbatim), plugin/src/action_sink.h, plugin/test/router_test.cpp (FakeSink pattern), plugin/test/fake_project.h.
    Create plugin/src/verb_registry.h: `enum class BindingGesture{Press,Tap,Double,Turn}` + `GestureName()` ("press"/"tap"/"double"/"turn"); `struct VerbContext{IActionSink& sink; IProject& project;}`; `class Verb{virtual void Run(VerbContext&); Begin(VerbContext&); Step(VerbContext&,int); End(VerbContext&) — all default no-op}`; `struct VerbBuildResult{std::unique_ptr<Verb> verb; std::string error;}` (verb null iff error non-empty); `struct VerbSpec{name; std::vector<BindingGesture> gestures; std::string description; factory(std::vector<std::string> args)->VerbBuildResult}`; `class VerbRegistry{Register, Find, Names, Build(name,gesture,args)}`; `VerbRegistry MakeDefaultRegistry()`.
    Build(): unknown name -> error `unknown verb 'X'`; gesture not in spec.gestures -> `verb 'X' does not accept gesture 'y'`; else run the factory (factory errors must start with the verb name, e.g. `take: ...`).
    plugin/src/verb_registry.cpp: register 10 verbs, logic moved verbatim from verbs.cpp: `action <id>` (RunCommand; one int arg), `targeted <id>` (Undo_Begin + WithTargetSelected{RunCommand(id)} + Undo_End("Speed Reaper: targeted <id>")), `ripple_delete` (RiplDel), `select_cursor_item` (TimelineTap), `take <N>` (N>=1; CamSetTake with N from the arg; label "Speed Reaper: take N"), `volume_readout` (AudioLevelTap), `smooth_cut` (SmthCutTap), `place_on_top` (PlaceOnTopTap), `isolate_tracks` / `solo_tracks` (VideoOnlyTap/AudioOnlyTap; each verb owns its own IsolateView/SoloView member).
    Gestures: action+targeted = press,tap,double; ripple_delete+take = tap,double; the other six = tap. Add src/verb_registry.cpp to PLUGIN_PURE_SOURCES in plugin/CMakeLists.txt (tests are auto-globbed: no test CMake edit).
    plugin/test/verb_registry_test.cpp: every one of the 10 names resolves and Builds; wrong gesture rejected (e.g. take on turn, smooth_cut on press); bad args rejected with a non-empty message (take 0, take abc, action abc, take with no arg, unknown verb). Do NOT add select_step / nudge / wheel / transport yet.
    Verify: `tools/check verb_registry`
  - [x] Turn verbs: select_step + nudge
    Extend plugin/src/verb_registry.cpp (no header change) with the two `turn` verbs, logic verbatim from plugin/src/verbs.cpp — read the HoldStart/HoldRotate/HoldEnd cases of OnGesture, kHoldRows, TimelineHoldRotate, StepSelection.
    `select_step` (Turn): per-verb `int select_delta_ = 0`; Step accumulates delta and for each ±20 ticks (keep `kSelectStepTicks = 20`) calls the moved-verbatim StepSelection (next/prev item on target track, clamped, exclusive selection).
    `nudge <move|trim_in|trim_out|slip_src|slip_dest|volume>` (Turn): per-verb state {targets, selection snapshot, `HoldNudge nudge{4}`, active flag}. Begin = HoldStart verbatim (no targets -> Trace("target none"), stay inactive, no undo block); Step = HoldRotate verbatim (one whole step per 4 ticks, step size 1/Fps(), apply the mode's ItemGeom function from nudge.h to each target); End = HoldEnd verbatim (restore snapshot, Undo_End with the kHoldRows label: "Speed Reaper: move|trim in|trim out|slip src|slip dest|audio level"). Bad args (nudge foo, wrong count) -> error starting with `nudge:`.
    Extend plugin/test/verb_registry_test.cpp: both names resolve (nudge under all 6 modes); select_step: 20 ticks selects the first item, the next 20 the next one, 10+10 ticks also steps (remainder kept); nudge: one undo block per hold, 1/fps step per 4 ticks, sub-threshold delta no-op, selection restored on End, no targets -> no undo block and no-ops.
    FakeProject::SetSelectedItems is recorded, not applied: after a selection call, copy the last recorded selection into items[].selected before the next verb call (pattern in plugin/test/verbs_test.cpp).
    Verify: `tools/check verb_registry`
  - [x] wheel + transport play_stop (shared WheelState)
    Add to plugin/src/verb_registry.h: `struct WheelState{WheelMode mode = WheelMode::Jog; WheelMode shuttle_prev = WheelMode::Jog; bool mode_change_pending = false; bool Set(WheelMode m); bool TakeModeChange(WheelMode *m);}` — Set: re-pressing the active mode is not a change, otherwise set mode_change_pending; TakeModeChange: true exactly once per change (consumed), *m gets the new mode. Owned by VerbRegistry (make the registry non-copyable); the wheel/transport factories capture a `WheelState*`. Do NOT include the SHTL dead-band HardStop state — that stays in Router::OnShuttleOffset (unwired, unchanged).
    Extend plugin/src/verb_registry.cpp with two press verbs, logic verbatim from Router::OnButton in plugin/src/router.cpp: `wheel <jog|scrl|shtl>` — a jog/scrl press from Shtl first calls sink.ApplyShuttleRate(1.0); an shtl press remembers the prior mode when not already in Shtl; `transport play_stop` — in Shtl: sink.HardStop() + Set(shuttle_prev), else sink.RunCommand(40044).
    Bad args -> error starting with the verb name: wheel foo, wheel with no arg, transport foo, transport with no arg, extra args.
    Extend plugin/test/verb_registry_test.cpp: both names resolve (press only; tap/turn rejected); press shtl -> mode Shtl + TakeModeChange once with Shtl; play_stop from Shtl -> HardStop + mode returns to Jog; press jog from Shtl -> ApplyShuttleRate(1.0) + mode Jog; play_stop outside Shtl -> RunCommand(40044).
    Verify: `tools/check verb_registry`

### 2. Bindings parser (pure)

- `plugin/src/bindings.h/.cpp`: `ParseBindings(std::string_view text) -> {Tuning, std::map<(key, gesture), BoundVerb>, std::vector<std::string> errors}` using the registry. INI as above: `;`/`#` comments, blank lines, whitespace-tolerant, unknown section → error line.
- `bindings_test.cpp`: numeric id, `_RSabc`, `targeted`, unknown key, unknown gesture, unknown verb, `press` + `tap` on one key, tuning values + defaults, derived double-tap set, errors carry line numbers.

**Verify**
```bash
tools/check bindings
```

- [x] Done

### 3. Default bindings = today's behavior

- `plugin/src/default_bindings.ini`, embedded at build time (CMake `file(READ)` → generated header). It reproduces exactly the current `kVerbTable`, `CompositeRows`, `kHoldRows`, `VerbDoubleTapKeys`, and `Router` key rules — one commented line each, comment = what it does in REAPER terms.
- Golden test `default_bindings_test.cpp`: parses with zero errors; derived double-tap set equals today's `{IN, OUT, ESC, TIMELINE, AUDIO_LEVEL}`; every (key, gesture) today's tables map is bound to the matching verb.

**Verify**
```bash
tools/check default_bindings
```

- [x] Done

### 4. Switch over and delete the old tables

- `Session` builds a `BindingTable` from the file (or the embedded default if missing/unreadable) and routes **every** key through `GestureEngine` → table → verb. `press` bindings fire on the down edge (gesture engine emits `Press` for keys whose only binding is `press`). Delete `kVerbTable`, `CompositeRows`, `kHoldRows`, `VerbDoubleTapKeys`, `Router::OnButton` key handling, and the key `switch` in the session. `Router` keeps only wheel mode state + shuttle law (rename if it is now just that).
- First run writes `<resource>/speed-reaper/bindings.ini` from the default. Add `REAPERAPI_WANT_GetResourcePath`, `NamedCommandLookup`.
- Split `verbs_test.cpp`: verb behavior tests move to `verb_registry_test.cpp` (they construct verbs directly); routing tests run through the default bindings. Assertions unchanged.

**Verify**
```bash
tools/check && ! rg -q 'kVerbTable|CompositeRows|kHoldRows|VerbDoubleTapKeys' plugin && tools/reaper-oracle --layer reaper
```

- [x] Done

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

- [!] Done
  BLOCKED: ralph could not replan this task (no valid JSON decision block); see vibes/.ralph/41-action-registry.md

### 6. Learn action

- `speed_reaper_learn` — `PromptForAction` session (open, poll from the timer, close) to pick an action; console prompts "press a device key + gesture (tap / double / hold+turn)"; the next gesture is written as `KEY.gesture = action <id or named>` into the file (replace an existing line for that key+gesture in place, else append to `[bindings]`), then reload. ESC on the device during learn cancels (ESC itself is not learnable during learn). 10 s timeout cancels.
- Pure part (line rewrite in the INI text, preserving comments) unit-tested in `bindings_test.cpp`. `PromptForAction` is not headless: add SMOKE-TEST.md "Bindings" section with the hand checklist (learn → fire → restart REAPER → still bound; edit file + reload; delete file → default rewritten).

**Verify**
```bash
tools/check bindings && grep -q '^## Bindings' SMOKE-TEST.md
```

- [x] Done

### 7. More `turn` handles + `step=`

- Implement `step=` (frame / grid via snap settings / sample / `<N>ms`) for all `nudge` verbs; default unchanged (`frame`).
- New nudge targets: `fade_in`, `fade_out` (D_FADEINLEN/D_FADEOUTLEN, clamped ≥ 0 and ≤ item length), `roll` (the edit point between the cursor item and its right neighbor on the same track: both edges move, total length fixed; no neighbor → `target none`), `sel_in`, `sel_out` (time selection edges; none → no-op).
- Unit tests in `nudge_test.cpp`; one oracle scenario `hold_handles` (fake holds a key bound to each new verb, asserts geometry). Default bindings: `IN.turn = nudge sel_in`, `OUT.turn = nudge sel_out`, `ROLL.turn = nudge roll`, others commented-out suggestions.

**Verify**
```bash
tools/check nudge && tools/reaper-oracle --layer reaper --scenario hold_handles
```

- [x] Done

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
