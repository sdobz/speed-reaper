# M6 oracle scenario rules

Each scenario has its own `oracle/fake/<name>.txt` + `oracle/scenarios/<name>.env` + `<name>.lua`, modelled on `verbs.lua` / `target.lua`.

- Run `tools/check` first if plugin source changed: `--layer reaper` does NOT rebuild the `.so`.
- Notes in `docs/notes/m5-observations.md` (## M6) are authoritative: `P_NAME` for track names, items select via `B_UISEL`, `EnumProjectMarkers` is 0-based, `CountMediaItems(nil,tr)` is the project total, `GetSet_LoopTimeRange2(nil,true,false,s,e,false)` sets the time selection.
- An uncaught Lua error inside a `defer` callback silently kills the chain (no probe.json → "scenario timeout"). Wrap risky calls in `pcall`. The oracle's scenario cap is 20 s: keep scenarios under ~6 s.
- A scenario passes or it does not. If a check still fails after two honest attempts, mark `- [!]` with a `BLOCKED:` line stating the measured values, rather than writing more probe scripts.
- Do not loosen checks. Do not leave `probe_*.lua` files behind.
