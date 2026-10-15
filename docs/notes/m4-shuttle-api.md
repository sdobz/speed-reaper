# M4 shuttle: REAPER playrate API spike — findings

Spike: a since-deleted one-off scenario under `tools/reaper-oracle --layer
reaper --report-only` (artifacts in `build/oracle/`:
`result.json`, `trace_spike_rate.txt`). Each spike action
(`speed_reaper_spike_rate2/rate8/reverse/reset`, plugin/src/main.cpp, M4
task 2) was run via `Main_OnCommand(NamedCommandLookup(...), 0)`, waited
600 ms, then the check details recorded the plugin's `spike …` trace
values, `reaper.Master_GetPlayRate(0)`, `reaper.GetPlayState()`, and two
samples 300 ms apart.

**Measured on** REAPER 7.81 linux-x86_64, headless (Xvfb, dummy audio,
empty project, edit cursor at 10 s). The edit cursor
(`GetCursorPosition`) does not follow the playhead during playback, so
"moved / reversed" is judged on the playhead (`GetPlayPositionEx(0)`),
sampled at t≈300 ms and t≈600 ms after the action.

## Measurements (one row per action)

| action | requested rate | read-back (`Master_GetPlayRate`) | play state | playhead t≈300 ms | playhead t≈600 ms | cursor moved? | reversed? |
| --- | --- | --- | --- | --- | --- | --- | --- |
| rate2  | +2.0 | **2.00** | 1 (playing) | 10.0615 s | 10.6705 s (+0.609 s ≈ 2.0×) | yes, forward | no |
| rate8  | +8.0 | **4.00** | 1 (playing) | 11.2752 s | 12.5361 s (+1.261 s ≈ 4.2×) | yes, forward | no |
| reverse | −1.0 | **0.25** | 1 (playing) | 13.9341 s | 14.0159 s (+0.082 s ≈ 0.27×) | yes, forward | no |
| reset  | +1.0 | **1.00** | 0 (stopped) | 14.0159 s | 14.0159 s | no | n/a |

Raw trace lines (plugin-side, ~300 ms after each action):
`spike rate=2.000000 read=2.000000 state=1`, `spike rate=8.000000
read=4.000000 state=1`, `spike rate=-1.000000 read=0.250000 state=1` +
`spike reverse pos0=12.536054 pos1=13.944172` (forward), `spike
rate=1.000000 read=1.000000 state=0`.

## Answers

- **Does 8× play?** No. REAPER clamps the master playrate to its standard
  0.25×–4.0× range: `CSurf_OnPlayRateChange(8.0)` plays, but
  `Master_GetPlayRate` reads back **4.0**, and the playhead moves at ~4×
  (1.261 s in 300 ms), not 8×. The transport plays fine — only the top of
  the range is unreachable.
- **Does a negative rate reverse?** No. `CSurf_OnPlayRateChange(−1.0)` is
  clamped to the **0.25** floor: read back 0.25, playhead still moves
  *forward* at ~0.25×. There is no reversed playback via the playrate API.
- **What is read back?** Exactly the clamped rate: 2.0 → 2.0, 8.0 → 4.0,
  −1.0 → 0.25, reset → 1.0. The clamp is symmetric (floor 0.25, ceiling
  4.0) and `Master_GetPlayRate(nullptr)` always reflects it, so read-back
  is a reliable source of truth for the current effective rate.

## Limits

Headless REAPER has no audio device (dummy audio under the oracle), so
audio quality and the scrub *sound* at any rate are unmeasured — that is a
human-test item only. The clamp itself is a REAPER-core limit (the
transport rate range), not a headless artifact, so it will hold on
real hardware. Timing figures carry ~30 ms sampling jitter (defer/timer
granularity); use the read-back rates, not the position deltas, as the
precise numbers.

## Gotchas found while running the spike

- `Main_OnCommand(40045)` (transport stop command) **blocks forever in
  headless REAPER** — the whole script/defer queue freezes (only the
  plugin timer thread keeps running) and the scenario times out (repro:
  `oracle/scenarios/dbg8.lua`). The spike plugin therefore stops via
  `OnStopButton()` (API subset §2), which returns and actually stops
  (verified: playstate 1 → 0, `oracle/scenarios/dbg9.lua`). Use
  `OnPlayButtonEx` / `OnStopButton` for transport control in the plugin,
  not 40044/40045.
- `reaper.GetCursorPosition()` is the *edit* cursor; it does not follow
  the playhead during playback. For motion checks use
  `GetPlayPositionEx(0)`.

## Candidate approaches (not implemented)

The design law (design §8) wants ±1…±8 with smooth reversal; the API only
offers +0.25…+4.0. Options:

1. **Playrate only.** Map the law onto `CSurf_OnPlayRateChange` as-is.
   Pros: zero added machinery, sample-accurate motion, honest transport
   state. Cons: covers only +1…+4 of the law — the 4…8× outer half and all
   of the negative half are unreachable, so the control law would have to
   be re-specified (e.g. rescale 4096 offset → 4×). Not a real shuttle.
2. **Playrate forward + stepped seek for reverse.** Forward: playrate
   (law clamped at 4×, or rescaled). Reverse: while the wheel offset is
   negative, stop playback (or keep it) and step the playhead backward in
   the plugin's ~30 ms timer — seek by `rate·dt` seconds each tick (e.g.
   via the cursor/position APIs used by the existing jog engine's sink).
   Pros: full ±8× range, both directions feel like one control; the
   playrate read-back stays truthful for the forward half. Cons: reverse
   motion is discrete (30 ms steps ≈ 33 fps of playhead jumps) — scrub
   *sound* will be choppy or silent (human-test item) and the playhead may
   visibly stutter; seeking at 0 s needs a clamp; the transport state
   (state 0) will disagree with the jog/scrub modes' expectations.
3. **Forward-only shuttle for v1.** SHTL does 1…4× (or the law rescaled to
   4×) forward, negative offsets do nothing (or nudge-stop at center).
   Pros: simplest, no stutter, everything the API supports is honest;
   covers the most common "sped up" use case; reverse can land later once
   a human decides whether stepped-sound is acceptable. Cons: asymmetric
   with the design law; users who expect a real shuttle get half of one.

Grounding: row 2 of the table kills pure "playrate only" for the full law;
row 3 shows reversal is *impossible* via rate (forward at 0.25×), which is
what forces approach 2's stepped seek or approach 3's feature cut.
Decision is the M4 human checkpoint (`vibes/23-m4-shuttle.md`); nothing is
implemented.
