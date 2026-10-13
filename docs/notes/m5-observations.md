# M5 observations (oracle findings)

- **`GetSet_LoopTimeRange2`, not `GetSet_Loc`**: real REAPER 7.81 exports `GetSet_LoopTimeRange2`; `ReaperProject::HasTimeSelection` uses it (`isSet=false`).
- **Item selection property is `B_UISEL`**, not `B_SELECTED` (which always reads 0 for items). `target.lua` originally used `B_SELECTED`, which made every "selection unchanged / A selected" check vacuous or failing; fixed to `B_UISEL`.
- **Undo needs an undo point after API-built setup.** Items added with `AddMediaItemToTrack` have no undo point, so Undo (40029) rolled back to the empty project. `target.lua` calls `Undo_OnStateChange("setup")` after setup. The RIPL DEL composite's undo block itself was fine (40310/40006/40309 in one block undoes in one step).
- **RIPL DEL with a time selection (40201)** on [1,2] inside A [0,5] splits A: A [0,1], A' [1,4], B [4,9]. Expected values in `target.lua` corrected (was "2 items"); no check loosened.
- After RIPL DEL (ripple off) B [5,10] has length 5 at pos 0 (check was wrongly `l0 == 10`).
- **Gotcha:** `tools/reaper-oracle --layer reaper` does NOT rebuild the plugin; only `--layer host|all` runs the cmake build. Run `tools/check` (or `--layer all`) first after plugin source changes, or you test a stale `.so`.
- `build/oracle/trace_<scenario>.txt` accumulates across runs; read the last block only.
