# Ralph record: 19-oracle-device-windows

How this checklist got done: attempts, facts learned, replan decisions.
Workers update their own `## Task:` section; ralph writes results.

## Learnings

Durable facts every task should know (keep short).


## Task: 1. `IHidTransport` seam under the driver

### Attempt 1 — 2026-10-12T03:31:39Z — budget 600s

- Facts: design §6 interface is `Open/Close/Read/Write/SendFeature/GetFeature/Serial` (PascalCase). Build files touching the driver: only `src/Makefile` (SRCS) and `plugin/CMakeLists.txt` (Linux `PLUGIN_PURE_SOURCES`). `SPEED_EDITOR_VID/PID` live in `src/speed_editor/key_map.h`. HidapiTransport keeps enumeration + serial storage; driver keeps a `bool opened` flag (replaces the old `dev != nullptr` guards) and stderr diagnostics — behavior identical. `buf[0]=reportId` for feature reads moved into `HidapiTransport::GetFeature`.
- Facts: `src/cli/speedcli.cpp` `cmdScan` calls `hid_enumerate`/`hid_free_enumeration` directly (previously pulled in via `hid_device.h`); added `#include <hidapi/hidapi.h>` there — CLI keeps direct enumeration, seam only covers the driver.
- Facts: builds need the nix shell (`tools/check` self-launches `nix develop`); verify passed: tools/check green (plugin -Werror, 2 unit tests, ORACLE PASS host), `speedcli scan` exits 1 (no device, ≤1 allowed). Line 21 ticked.
