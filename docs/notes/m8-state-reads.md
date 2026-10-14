# M8 state reads — measured `GetToggleCommandState` values (REAPER 7.81)

Measured by `oracle/scenarios/led_reads.lua` (no fake device; `settled` start,
fresh profile per run — see `build/oracle/probe_led_reads.json`). Lua:
`reaper.GetToggleCommandState(cmd)` returns `0` = off, `1` = on, `-1` = NA
(action has no on/off state). All values below measured after each
`Main_OnCommand`, one defer cycle later.

## Ripple mode (set with the "Set ripple editing ..." actions)

| state (set by)            | `40310` per-track | `40311` all | `41990` toggle per-track | `41991` toggle all | `1162` on/off |
|---------------------------|:---:|:---:|:---:|:---:|:---:|
| off (40309)               | 0 | 0 | 0 | 0 | 0 |
| per-track (40310)         | 1 | 0 | 1 | 0 | 1 |
| all (40311)               | 0 | 1 | 0 | 1 | 1 |

Surprise: the **"Set" commands `40310`/`40311` are valid toggle queries** —
REAPER reports 1 when their mode is active, 0 otherwise. The dedicated
"Toggle ripple editing per-track/all" actions `41990`/`41991` (Main section,
`docs/reaper-actions.tsv`) track identically. There is no Get/Set Lua API for
the ripple mode (grep of `reaper-api.htm`); `GetToggleCommandState` on these
actions is the only programmatic read. Design §5 candidate ids are therefore
**correct as written — no fix needed**: ripple 0 → `40310==0 and
40311==0`, ripple 1 (CUT) → `40310==1`, ripple 2 (DIS) → `40311==1`.

## Toggles (value per setting)

| setting        | query                          | measured (before → after toggle) |
|----------------|--------------------------------|----------------------------------|
| snap           | `GetToggleCommandState(1157)`  | 1 → 0 (fresh-profile default: ON) |
| trim-behind    | `GetToggleCommandState(41117)` | 0 → 1 (fresh-profile default: OFF; read current state first, then toggle) |

Both track exactly (`after == 1 - before`); neither returned `-1`. Note the
fresh-profile defaults differ (snap on, trim-behind off) — read, don't assume.
`41116` (cycle auto-crossfade off/on/trim-behind) and `41118`/`41119`
(enable/disable auto-crossfades) are write-side alternatives to `41117`;
`41117` is the direct toggle and the right one for a read-modify-write.
