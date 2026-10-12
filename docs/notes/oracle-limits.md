# Oracle limits & REAPER-behavior findings (headless Linux REAPER 7.81)

## Jog strategy (docs/reaper-plugin-workflow.md §3)

- **`CSurf_ScrubAmt` does not move the edit cursor when the transport is
  stopped** (called from Lua: 5×0.5 → 0.00000; from the plugin one call gave a
  fixed 0.005 regardless of amount/count). No documented unit. Not usable for
  the stopped-JOG job.
- **`SetEditCurPos(GetCursorPosition()+dt, false, false)`** moves the cursor
  exactly and linearly (100×0.003 → 0.30000). This is the stopped-JOG
  primitive; while playing the sink seeks with `SetEditCurPos(play+dt,false,true)`
  (untestable headless: no audio device → `GetPlayState()` stays 0).
- **`CSurf_OnSeek` is not an exported REAPER function** (docs §3 names it;
  use `SetEditCurPos`/`SetEditCurPos2`).
- Plugin `timer` registration takes only `void(*)()`; REAPER calls it at
  ~30 Hz (≈32 ms), not the period we asked for. The plugin catches up in fixed
  8 ms engine steps from a steady clock.
- Demo displacement is 0.580 s with `kMaxDeltaPerTick = 53` (design ≈ 0.5 s).

## Harness gotchas

- **Modal startup dialogs freeze plugin timers.** A fresh profile shows
  "Error opening devices", the evaluation "About REAPER" nag and "New Version
  Notification". While open, REAPER stops calling plugin timers (Lua defer
  still runs) and Quit hangs. `tools/reaper-oracle` dismisses them with
  xdotool `key Return`; closing the X window instead wedges REAPER's modal loop.
- `xvfb-run` costs ~3 s per call; the harness starts one Xvfb. In this sandbox
  the X socket is abstract (`/proc/net/unix`), not under `/tmp/.X11-unix`.
- Lua: `reaper.get_time` / `GetSetTransportInfo` do not exist (`time_precise`;
  `GetPlayState`). Scenario bodies run under `run(...)` so Lua runtime errors
  fail fast; a *syntax* error still costs the scenario timeout.
