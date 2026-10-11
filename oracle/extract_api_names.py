#!/usr/bin/env python3
"""Extract the REAPER 7.81 exported API function name list.

- Reads the "Complete function index" section of docs/reaper-api-subset.md
  (the backticked names, 4 per line) and writes them, one per line, to
  oracle/reaper_api_names.txt.
- Prints to stdout: the count, plus the SDK function pointers declared in
  vendor/reaper-sdk/sdk/reaper_plugin_functions.h (`(*REAPERAPI_FUNCNAME(Name))(`)
  that are missing from the list. Redirect that stdout to
  docs/notes/sdk-vs-reaper-api.md when generating the note.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOC = ROOT / "docs" / "reaper-api-subset.md"
SDK_HDR = ROOT / "vendor" / "reaper-sdk" / "sdk" / "reaper_plugin_functions.h"
OUT = Path(__file__).resolve().parent / "reaper_api_names.txt"


def extract_names(doc_text: str) -> list[str]:
    m = re.search(r"^##\s*Complete function index", doc_text, re.M)
    if not m:
        raise SystemExit(f"error: '## Complete function index' not found in {DOC}")
    section = doc_text[m.end():]
    # stop at the next markdown heading, if any
    nxt = re.search(r"^##\s", section, re.M)
    if nxt:
        section = section[: nxt.start()]
    return re.findall(r"`([A-Za-z0-9_]+)`", section)


def extract_sdk_names(hdr_text: str) -> list[str]:
    names = re.findall(r"\(\*REAPERAPI_FUNCNAME\(([A-Za-z0-9_]+)\)\)\(", hdr_text)
    seen, out = set(), []
    for n in names:
        if n not in seen:
            seen.add(n)
            out.append(n)
    return out


def main() -> int:
    names = extract_names(DOC.read_text(encoding="utf-8"))
    OUT.write_text("\n".join(names) + "\n", encoding="utf-8")

    sdk_names = extract_sdk_names(SDK_HDR.read_text(encoding="utf-8"))
    missing = [n for n in sdk_names if n not in set(names)]

    print(f"# SDK vs. REAPER 7.81 API name list")
    print(f"""
Extracted from the "Complete function index" section of `docs/reaper-api-subset.md`.

- API functions in the list: **{len(names)}** (written to `oracle/reaper_api_names.txt`)
- SDK pointer declarations in `vendor/reaper-sdk/sdk/reaper_plugin_functions.h`: **{len(sdk_names)}**
- SDK declarations missing from the list: **{len(missing)}**
""")
    if missing:
        print("Missing from the list:")
        print()
        print("```text")
        print("\n".join(missing))
        print("```")
    else:
        print("None — every SDK declaration appears in the list.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
