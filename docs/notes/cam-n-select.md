# CAM N: `select_track` contract (vibes/45)

Builds on `docs/notes/led-binding.md`.

## Decisions (settled — do not reopen)
- Verb `select_track <N>`, N in 1..9, tap only; error text `select_track: N must be 1..9`. Toggles the selection bit of the item under the cursor on track N (1-based); the rest of the selection is untouched; no item → no-op + `Trace("target none")`; no undo block.
- Its `state_read` provider returns: item under cursor on track N exists **and** is selected. The key's identity plays no role (CAM1 bound to `select_track 3` follows track 3).
- Per 43's mask, the binding owns the key's LED (no OR with anything). Any LED-bearing key may be bound to it; no CAM-specific code anywhere.
- CAM defaults change `take N` → `select_track N`. `take` stays registered and bindable (user files keep working; their CAM LEDs still follow `take N`'s read). No migration of user `bindings.ini` (design §5).
- A key bound to a verb with no provider is dark (43).


## Native candidates (`rg -i 'select' docs/reaper-actions.tsv`, narrowed to item select/toggle)
| id | name | headless (measured, REAPER 7.81, `--scenario cam_probe`) |
|----|------|------|
| 40528 | Item: Select item under mouse cursor | no-op (mouse-cursor based) |
| 40529 | Item: Select item under mouse cursor (leaving other items selected) | no-op (mouse-cursor based) |
| 40530 | Item: Toggle selection of item under mouse cursor | no-op (mouse-cursor based) |
| 40416 | Item navigation: Select and move to previous item | works; selects the item at the edit cursor (track 1's item [0,2] got `B_UISEL=1`) and **moves the cursor** to the item start (1.0 → 0.0); sticky (2nd run = no change) |
| 40417 | Item navigation: Select and move to next item | no-op at cursor 1.0 (no item starts at/after the cursor); exclusive select + cursor move by name |

Measured facts from `cam_probe` (REAPER 7.81, two tracks, one item each [0,2], cursor 1.0):
- (a) `SetMediaItemInfo_Value(it, "B_UISEL", 1/0)` on the track-2 item works and leaves the track-1 item's `B_UISEL`/`B_SELECTED` untouched in both directions.
- (b) The edit cursor does not move on a `B_UISEL` set or clear (1.0 → 1.0).
- The three mouse-cursor candidates (40528/40529/40530) are no-ops headless: the mouse is not over an item, so toggle-vs-exclusive and target item could not be classified in the oracle.
- This REAPER Lua export lacks `AddEmptyItem`, `SetEditCursor`, `GetEditCursor`; use `AddMediaItemToTrack`+`AddTakeToMediaItem`, `SetEditCurPos(pos, false, false)`, `GetCursorPosition()`.

## Holes
- **H1 — does a native per-track "select/toggle item at the edit cursor" exist? No.** No alternative id to record. The closest native toggle, 40530 "Item: Toggle selection of item under mouse cursor", targets the *mouse* cursor, not the edit cursor, is not per-track, and is a no-op headless (see table). `select_track` must be implemented via the API (`B_UISEL` on the item under the edit cursor on the named track), as the Decisions state; the Decisions are confirmed, not contradicted.
- **H2 — "cursor at item end" for a named track cannot be probed headless** unless H1 had yielded a native per-track action (it did not): nothing in the oracle can position the edit cursor deterministically on a given track's item boundary without moving it as a side effect (40416/40417 move the cursor). The unit test in step 2 (`CursorItemOnTrack`) is the arbiter.
