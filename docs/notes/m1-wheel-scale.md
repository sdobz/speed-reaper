# M1 wheel scale & behaviour — findings from the real device

Source: `/mnt/c/sr_trace.txt` hand tests on Windows (real Speed Editor, JOG
mode = protocol mode 0, REAPER stopped).

## Measured

- Relative-mode reports (`03 00 <i32 LE> ff …`) carry a **per-report delta**,
  not a position; 0 when the wheel is still. Reports arrive every ~16–24 ms.
- Smallest step = **360 units**.
- One full revolution ≈ **1.46–1.50 M units** (slow CW rev: +1,460,078 in
  3.4 s; fast CCW rev: −1,504,796 in 0.48 s) ≈ 4096 detents × 360 =
  **1,474,560 units/rev**.
- Rates: slow turn ≈ 0.4 M units/s (15–29 k per report); fast turn ≈ 3 M
  units/s (peak 86 k per report, ~2 rev/s).

## What was wrong (all fixed)

1. **Trace stopped at 509 lines**: `Trace()` leaked a `FILE*` per call; the
   Windows CRT allows 512 open streams. Now `fclose`d; regression test writes
   2000 lines.
2. **Scale 0.0001 s/unit** (a guess) made one revolution = 147 s, 
   and a detent = 36 ms.
3. **Per-tick cap of 60 units + unbounded carry** in `HidWheelSource::Tick`:
   real reports are up to ~1000× larger than the cap, so the backlog (peak
   1.43 M ≈ one revolution) replayed at a fixed rate for minutes after the
   wheel stopped — it behaved like a throttle whose direction was the running
   sum of all turns.
4. **Velocity smoothing (τ = 50 ms)** in `JogEngine` was not asked for by
   `docs/archive/reaper-plugin-workflow.md` §3 (small deltas = nudges, larger =
   proportional); it added glide/coast the user did not want when stopped.

## Decisions

- **Carry is not needed.** Its purpose was to avoid a one-tick burst after a
  descheduled worker. A burst equals real wheel travel, so applying it
  immediately is correct; REAPER's timer catch-up is already bounded in
  `main.cpp` (`kMaxCatchUpMs`). Removed with the cap.
- **No smoothing.** `JogEngine::Tick` moves the playhead by
  `delta × kSecondsPerWheelUnit`; it stops when the wheel stops (position
  control, not velocity).
- **Scale: 20 s of timeline per revolution** (`kSecondsPerRev`), i.e.
  1.356e-5 s/unit: 1 detent (360 u) ≈ 4.9 ms, 1° ≈ 55 ms, ~1 frame @30 fps
  per 0.6° of rotation; a fast 2 rev/s flick ≈ 40 s/s. Chosen as a
  coarse/fine compromise; **tune by hand** (needs a real-device feel test).

## Candidate user parameter

`jog.seconds_per_rev` — default 20, range ~5–60 (also see
`docs/archive/plugin-configuration.md` §2.2). `kUnitsPerRev` is hardware, not tunable.
Possible later: a second, finer scale with a modifier key; acceleration
(speed-dependent gain).

## Still open

- SHTL (absolute, mode 3) is a separate, planned feature (M4).
- Playing-transport behaviour of the new scale is untested on a real device.
