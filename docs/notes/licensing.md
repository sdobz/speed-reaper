# Licensing

The repo root `LICENSE` is **BSD-3-Clause** (Copyright (c) 2026,
Vincent Khougaz) and covers the project's own code: `plugin/`,
`src/cli/` and friends, `tools/`, `oracle/`, `docs/`. Vendored
components are not re-licensed; they stay under their own terms with the
notices kept in-tree.

Why BSD-3-Clause: it is the identical license of the largest vendored
component (the ported Speed Editor driver), and every other *licensed*
vendored component is permissive (MIT / MIT-flavored), so no clauses
conflict. MIT would also be compatible; BSD-3 avoids mixing two
"effectively the same" licenses.

## Vendored component table

| Path | License | Compatible with BSD-3-Clause | Notes |
|---|---|---|---|
| `src/speed_editor/` | BSD-3-Clause, © 2026 John Howard (`src/speed_editor/LICENSE`) | Yes — same license | Port of the C++ driver from John-Howard/PythonSpeedEditor @ `28057b4`; notice kept in-tree, credits in README |
| `docs/speed-editor-examples/python-driver/` | BSD-3-Clause, © 2026 John Howard (own `LICENSE`) | Yes — same license | Reference driver, pinned at `28057b4` |
| `docs/speed-editor-examples/cpp-driver/` | BSD-3-Clause, © 2026 John Howard (own `LICENSE`) | Yes — same license | Reference driver, pinned at `28057b4`; basis of `src/speed_editor/` |
| `docs/speed-editor-examples/python-bmd/` | **None — all rights reserved** | Not re-licensed | Reference-only vendored copy (octimot/blackmagic-speededitor @ `0919407`); **no license can cover it** (we hold no copyright). Not ported into shipped code — `src/speed_editor/` derives from the BSD-3 `cpp-driver`, not from `bmd.py`. Upstream URL recorded in `docs/speed-editor-examples/README.md`. It sits outside the root LICENSE. |
| `vendor/reaper-sdk/` | MIT-flavored grant (`vendor/reaper-sdk/sdk/LICENSE`: use/alter/redistribute freely, keep notice) | Yes — permissive | Headers-only use (`reaper.h`, `reaper_plugin.h` as SYSTEM include); `reaper-plugins/` sources are not built. `reaper_mp3/` inside carries its own IJG/BSD-ish `LICENSE` — also unused. |
| `vendor/WDL/` | MIT (upstream justinfrankel/WDL); **the vendored copy ships without the LICENSE file** | Yes — permissive | Headers-only SYSTEM include root (`plugin/CMakeLists.txt:120`); no WDL `.cpp` is compiled or linked. Nested third-party subdirs (`giflib/`, `jpeglib/`, `libpng/`, `lice/glew/`) are likewise unused by the build. |

## Conclusion

No vendored *licensed* file is incompatible with a permissive choice, so a
permissive root license was picked (BSD-3-Clause). The one caveat is
`python-bmd/`: it has no license at all, so it cannot be released under any
permissive license (or ours). It is kept as reference-only documentation,
clearly attributed, and explicitly excluded from the root license in the
`LICENSE` footer.
