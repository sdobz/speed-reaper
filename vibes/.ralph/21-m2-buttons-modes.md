# Ralph record: 21-m2-buttons-modes

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).

- `tools/check` includes `tools/windows-oracle --layer static`, whose DLL comes from `nix build .#reaper-plugin-windows` — that builds from **committed** git sources. Commit new files before running it (untracked files → "Cannot find source file").
- SWELL (`swell-types.h`) defines `min`/`max` as function-like macros. Any NEW `<vector>`/`<string>`/etc. std include needed by a plugin header must be added to the pre-SDK std-include block at the top of `plugin/src/main.cpp` (existing pattern + comment), or the libstdc++ header's `std::max` breaks under -Werror.
- `tools/check` needs `nix develop` (cmake/g++ not on plain PATH); bgrun jobs: check1/2/3, win, winbuild2, hostorc.


## Task: 1. `DiffHeldKeys` (pure) and use it in the worker

### Attempt 1 — 2026-10-12T07:11:23Z — budget 1800s

- Facts: tree clean (no partial work). Design §6: `DiffHeldKeys(const vector<uint16_t>& prev, const vector<uint16_t>& cur, vector<ButtonChange>* out)`, releases first (prev order) then presses (cur order). Created `plugin/src/button_diff.{h,cpp}`, added `button_diff.cpp` to `PLUGIN_PURE_SOURCES` (plugin/CMakeLists.txt line ~48); moved `ButtonChange` to button_diff.h, hid_wheel_source.h includes it; removed private helper + `<algorithm>` from hid_wheel_source.cpp. Test at `plugin/test/button_diff_test.cpp` (tests glob `*_test.cpp`, link PLUGIN_PURE_SOURCES, keycodes from `src/speed_editor/key_map.h`: JOG=0x1D, SCRL=0x1E, SHTL=0x1C, STOP/PLAY=0x3C).
- Dead ends: first `tools/check button_diff` failed compiling `main.cpp`: `stl_bvector.h:1740 std::max(size(), __n)` error under -Werror. Root cause: SWELL (`swell-types.h:128`) defines `max`/`min` as function-like macros; the old main.cpp TU never included `<vector>`, but button_diff.h (via hid_wheel_source.h) pulled it in *after* `reaper_plugin.h`, so `std::max` in the libstdc++ header got macro-mangled. Fix (repo's established pattern, main.cpp top comment): added `#include <vector>` to the pre-SDK std-include block in main.cpp (task 1 forbids the Router, not this one-line include; comment updated).
- Facts: check2: build + `button_diff_test` PASS, but `tools/check` rc=1 in the next step `tools/windows-oracle --layer static` (no output). On a clean tree that step passes instantly (cached nix store DLL); with our source change it must re-run `nix build .#reaper-plugin-windows` (stderr discarded by the script) — likely that rebuild fails or is slow. Re-running it as bgrun job `win` to see the real error.
- Facts: real cause of the windows-oracle failure: `nix build .#reaper-plugin-windows` builds from committed sources; our new files were untracked → "Cannot find source file: button_diff.cpp". Committed (commit on main). After commit: winbuild2 PASS, `tools/reaper-oracle --layer host` PASS, `button_diff_test` PASS.
- Done: check3 (full `tools/check button_diff`) rc=0. Line 20 ticked. Router not written, per task.

Result: exit 0; done; log: logs/21-m2-buttons-modes-L20-a1-1791789083118.log

### Done — 2026-10-12 — 1 attempt(s)
Files:
```
?? vibes/.ralph/21-m2-buttons-modes.md
```

## Task: 2. `Router` and `ModeGatedSource` (pure)

### Attempt 2 — 2026-10-12T07:19:06Z — budget 1800s

- Facts: no partial work from attempt 1 (tree clean except this record). Design §6 (docs/plugin-design-m1-m3.md:92-103) gives all signatures. Keycodes in `src/speed_editor/key_map.h` (JOG 0x1D, SCRL 0x1E, SHTL 0x1C, STOP/PLAY 0x3C). `IWheelSource`/`WheelSample` in `jog_engine.h` (already in PLUGIN_PURE_SOURCES). Test files glob `*_test.cpp` and include `${REPO_ROOT}/src`, so `#include "speed_editor/key_map.h"` works in tests.
- Facts: created `plugin/src/wheel_mode.h` (ProtocolMode Jog=0/Scrl=2/Shtl=3), `plugin/src/action_sink.h` (IActionSink: RunCommand/ScrollView), `plugin/src/router.h` (Router + ModeGatedSource, kScrollUnitsPerStep=20, initial mode Jog, press rules, SHTL no-op, STOP/PLAY→40044, TakeModeChange once per real change), `plugin/src/router.cpp`, `plugin/test/router_test.cpp`; added `router.cpp` to PLUGIN_PURE_SOURCES. main.cpp untouched (router.h only pulls <cstdint>/<vector>, already pre-SDK-included).
- Dead ends: first `tools/check router` (job check21) failed only in `router_test`: ModeGatedSource negative-turn expectation ignored the scroll remainder 5 left by the previous 45-unit turn (5−40=−35 → 1 step, not 2). Fixed the test to use −45 (5−45=−40 → exactly 2 steps). Router logic itself was correct.
- Dead ends: real Router bug found by the test: negative accumulation used `while (remainder < -k)` which understeps exact multiples (remainder −40 gave 1 step, not 2). Fixed to `<=` in `router.cpp` OnWheel.
- Done: check23 (`tools/check router`) rc=0 — build clean under -Werror, `router_test` 1/1 pass, Windows oracle static PASS, host oracle PASS. Line 33 of vibes/21-m2-buttons-modes.md ticked. main.cpp untouched, per task.

Result: exit 0; done; log: logs/21-m2-buttons-modes-L33-a2-1791789546710.log

### Done — 2026-10-12 — 2 attempt(s)
Files:
```
M vibes/.ralph/21-m2-buttons-modes.md
```
