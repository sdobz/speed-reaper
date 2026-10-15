# Crossfade actions (REAPER 7.81, oracle spike)

Source: `tools/reaper-oracle --layer reaper --scenario spike_xfade --report-only`
(`oracle/scenarios/spike_xfade.lua`, recorded values only — no asserts), and
the asserting scenario `xfade_keys` for the chosen defaults. Items are audio
takes: **empty items ignore crossfades**. Layout: track 0 = A `[0,5]` + B
`[4,9]` (overlap `[4,5]`), track 1 = C `[0,5]` + D `[5,10]` (abutting). New
items start with 0.01 s manual fades. "auto" = `D_FADEOUTLEN_AUTO` /
`D_FADEINLEN_AUTO` (the crossfade length REAPER derives from the overlap;
1.0 = the 1 s overlap).

| id | name | recorded effect on fades / items |
|---|---|---|
| 41059 | Item: Crossfade any overlapping items | Selected items that overlap get a crossfade over the whole overlap: C `[0,5.5]` + D `[4.5,10]` → C fade-out auto = 1.0, D fade-in auto = 1.0. A/B (already overlapped by 0.02 s from an earlier step) → auto 0.02. Does not change item extents. |
| 40916 | Item: Crossfade items within time selection | Time selection `[4,5]` over the A/B overlap → crossfade flagged auto = 1.0 on A out / B in, extents unchanged. Time selection `[4.5,5.5]` over the abutting C/D → both items are **extended** into each other (C `[0,5.5]`, D `[4.5,10]`) and crossfaded (auto = 1.0). Needs ≥ 2 selected items. |
| 43776 | Item: Set crossfade length to default | Acts on **auto** crossfades only: A/B with auto flags → the overlap shrinks to the default length 0.02 s (A.len 4.51, B.pos 4.49, manual fade lengths 0.02). Manual fades (0.5 / 0.3) are left untouched. |
| 43777 | Item: Set crossfade shape to default | No change to lengths/positions (shape only). |
| 41191 | Item: Remove fade in | Selected items' fade-in length → 0 (manual fades). |
| 41192 | Item: Remove fade out | Selected items' fade-out length → 0. |
| 41193 | Item: Remove fade in and fade out | Both → 0 on all selected items. |
| 40509 / 40510 | Item: Fade items in to / out from cursor | Fade length from item edge to the edit cursor (B in 1.51 s, D 1.5 s; B out 3 s, D out 4 s). Not crossfades. |
| 41118 / 41119 | Options: Enable / Disable auto-crossfades | Config var `autoxfade` (string API) 129 ↔ 128: bit 0 = auto-crossfade on. `GetToggleCommandState` returns -1 for both (not toggle actions). |
| 41116 | Cycle editing modes | `autoxfade` 129 → 130 → 131 → 128 (cycles ripple/crossfade bits). |

Default crossfade length: not reachable via `get_config_var_string`
(`defaut_xfade` etc. absent in a fresh home); the 0.02 s default is observed
through 43776.

## Binding decisions (default_bindings.ini)

Rule: a key is bound only to an id whose recorded effect matches its label.

| key | id | why |
|---|---|---|
| `TRANS.tap` | `action 41059` | creates a crossfade over the selected overlapping items |
| `DIS.tap` | `action 40916` | creates a crossfade within the time selection (extends abutting items) |
| `CUT.tap` | `action 41193` | removes both fades (the crossfade) from the selected items |
| `TRANS_DUR.tap` | `action 43776` | sets the crossfade length to the default |
| `TRANS_DUR.turn` | *unbound* | `nudge fade_in` changes the manual `D_FADEINLEN`, not the auto crossfade length (43776 leaves manual fades alone), so it does not read as "crossfade length"; shipped as a commented suggestion for fade length |

Keys act on the **selected** items (plain `action`, not `targeted`): the
crossfade needs the overlapping pair selected, which the implicit single
target cannot supply. The oracle scenario `xfade_keys` pins all four.
