# M4 shuttle implementation decision (normative for `vibes/23-m4-shuttle.md` §5–6)

Input: `docs/notes/m4-shuttle-api.md` (playrate spike, REAPER 7.81 linux-x86_64,
headless oracle). Law: design §8 of `plugin-design-m1-m3.md` —
`ShuttleRate(offset)` ∈ {0 in dead band |offset|<40, ±1.0 at |offset|=40, ±8.0 at
|offset|≥4096, smoothstep between}.

## Decision

**Rule 2 wins: forward via playrate, reverse via stepped seek
(`SetEditCurPos` back by `rate·dt` each timer tick).**

Rule evaluation, in order:

1. *"playrate only" — rejected.* Requires 8× forward and −1× reverse both to
   run with the rate read back and the cursor moving in the right direction.
   Forward: +8.0 requested reads back **4.0** (REAPER clamps master playrate to
   0.25…4.0) and the playhead moves forward — forward half partially holds.
   Reverse: −1.0 requested reads back **0.25** and the playhead still moves
   *forward* at ~0.25× — reverse does not run. Rule 1 fails on reverse.
2. *"forward via playrate + stepped seek reverse" — **selected**.* Forward
   works (read-back 2.0 for 2.0, playhead advances at the read-back rate);
   reverse does not (clamped to the 0.25 floor, still forward). This is
   exactly the shape rule 2 covers.

Consequence for the law: the playrate API only reaches +0.25…+4.0, so the
forward half of `ShuttleRate` (which spans +1…+8) must be clamped to 4.0 at
the API boundary (`CSurf_OnPlayRateChange(std::min(rate, 4.0))`); the read-back
via `Master_GetPlayRate(nullptr)` is the effective rate and the transport
state is honest. The reverse half (−1…−8) is synthesized by seeking, so its
full range is reachable in cursor motion.

## Fixed implementation parameters

- **Rate setter (forward): `CSurf_OnPlayRateChange(double)`** (projectless,
  API subset §5; `REAPERAPI_WANT_CSurf_OnPlayRateChange` already present).
  Read-back: `Master_GetPlayRate(nullptr)` (reliable source of truth — the
  clamp is applied inside REAPER and always reflected). Never feed the
  requested rate downstream; feed the read-back.
- **Reverse seek: `SetEditCurPos(time, false, seekplay)`** — the same call the
  existing `ReaperPlayheadSink::ScrubBySeconds` already uses (main.cpp L128).
  Each engine tick while `ShuttleRate(offset) < 0`:
  `pos = GetPlayPositionEx(0)`;
  `SetEditCurPos(pos − (−rate)·dt, false, /*seekplay=*/1)` — i.e. the
  playhead steps **back** by `rate·dt` seconds per tick, `dt` = tick period
  below. Clamp `pos` to ≥ 0.0. Only seek when the transport is playing
  (read via `GetPlayState()`); if stopped, park the rate at 0 and re-arm on
  next STOP/PLAY press (reverse while stopped is scrub, which is JOG/SCRL's
  job).
- **Tick period: the existing engine timer — `kTimerMs = 8 ms` (125 Hz) fixed
  steps inside REAPER's ~30 Hz timer callback** (main.cpp L80, L206–208).
  Each timer callback advances the engine in 8 ms steps with the existing
  ≤50 ms catch-up cap, so reverse motion is 8 ms-granular (≈125 fps stepping)
  in the normal case and degrades gracefully under main-loop delay. Do not
  add a second timer.
- **SHTL press**: enter SHTL mode; transport must be playing for the rate to
  matter (if stopped, the wheel still drives the rate law, playback starts on
  STOP/PLAY press).
- **Dead band (|offset| < 40) → stop**: `ShuttleRate` returns 0; on 0, set
  `CSurf_OnPlayRateChange(1.0)` and call `OnStopButton()` (hard pause; the
  playhead holds its position). Do **not** rewind on center.
- **STOP/PLAY hand-off**: a STOP/PLAY press while in SHTL is a hard stop —
  `OnStopButton()`, `CSurf_OnPlayRateChange(1.0)`, exit SHTL mode (back to the
  prior mode, initial `Jog`). Per design §6 STOP/PLAY is press-semantics; per
  the M4 spike's gotcha, stop via `OnStopButton()`, never `Main_OnCommand(40045)`
  (blocks the script/defer queue in headless REAPER). A subsequent STOP/PLAY
  press starts playback (`OnPlayButtonEx(0)`) from the held position at rate
  1.0.
- **JOG-tap hand-off**: a JOG (or SCRL) press while in SHTL hands off to the
  pressed mode — `CSurf_OnPlayRateChange(1.0)` (rate back to 1.0) and the mode
  switches, so the wheel is again in relative jog/scrub motion (Jog/Scrl),
  not the absolute SHTL law. This is the symmetric "tap out of SHTL" exit.
- **Rate application cadence**: re-apply `CSurf_OnPlayRateChange` (forward) or
  the stepped seek (reverse) each engine tick from the current absolute
  `WheelEvent.offset` — SHTL is absolute, so there is no integration; the law
  is a pure function of the present offset.

## Known limitations (inherited from the spike, human-test item E2)

- Forward is capped at 4.0× read-back (REAPER core clamp, holds on real
  hardware). The 4…8× outer half of the law is flat at 4.0× in transport
  terms.
- Reverse motion is discrete stepping; scrub *sound* at reverse is unmeasured
  (headless, no audio device) and will likely be choppy or silent.
- The transport state during stepped reverse is "playing" while the playhead
  moves backward — consistent with jog/scrub expectations only via the
  read-back rate, so any UI/status reporting must use
  `Master_GetPlayRate(nullptr)` plus the mode, not `GetPlayState()` alone.

Nothing here is implemented; `vibes/23-m4-shuttle.md` task 5 wires it.
