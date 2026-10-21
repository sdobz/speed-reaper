# CAM N: `select_track` contract (vibes/45)

Builds on `docs/notes/led-binding.md`.

## Decisions (settled — do not reopen)
- Verb `select_track <N>`, N in 1..9, tap only; error text `select_track: N must be 1..9`. Toggles the selection bit of the item under the cursor on track N (1-based); the rest of the selection is untouched; no item → no-op + `Trace("target none")`; no undo block.
- Its `state_read` provider returns: item under cursor on track N exists **and** is selected. The key's identity plays no role (CAM1 bound to `select_track 3` follows track 3).
- Per 43's mask, the binding owns the key's LED (no OR with anything). Any LED-bearing key may be bound to it; no CAM-specific code anywhere.
- CAM defaults change `take N` → `select_track N`. `take` stays registered and bindable (user files keep working; their CAM LEDs still follow `take N`'s read). No migration of user `bindings.ini` (design §5).
- A key bound to a verb with no provider is dark (43).

