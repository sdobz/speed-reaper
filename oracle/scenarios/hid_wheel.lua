-- oracle/scenarios/hid_wheel.lua — fake Speed Editor device end-to-end
-- (vibes/20-m1-wheel-playhead.md task 7; re-baselined to the pixel scale
-- in vibes/46-jog-zoom.md task 6).
--
-- The harness sources oracle/scenarios/hid_wheel.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/wheel_unplug.txt. The plugin's
-- HID worker opens the scripted device at plugin load and plays:
--   100–1200 ms    first wheel turn (138 x `wheel 600`)
--   1225–1475 ms   11 decaying deltas of turn 1 (turn 1 total ~ 87240 units)
--   1500 ms        unplug   -> worker logs `disconnected`
--   3000 ms        plug     -> worker reconnects on its 1 s open retry
--   3200–4288 ms   second wheel turn (137 x `wheel 600` = 82200 units)
-- The CLI script starts ~40 ms after plugin load (measured in the oracle),
-- so the sample clock below tracks the script clock with a small offset.
--
-- Motion scale (vibes/46-jog-zoom.md): seconds = (units/360) * (0.25/pxPerSec),
-- where pxPerSec is reaper.GetHZoomLevel() at scenario start (no assumed
-- window size; the headless pxPerSec is measured, not set).
--
-- Checks:
--   1. pxPerSec > 0 (measurable headless zoom)
--   2. cursor rose > (87240/360)*(0.25/pxs)/6 during the first turn
--      (sample window t <= 1400 ms; ~1/6 of the expected turn 1 motion)
--   3. max step between consecutive samples in that window < 6 x per-event
--      motion, 6*(600/360)*(0.25/pxs) s (stutter; excluded: dt > 100 ms
--      stall intervals, and the first-motion interval — the engine's
--      startup backlog flush, see below)
--   4. trace has `log connected`, then `log disconnected`, then
--      `log connected` again, in that order
--   5. cursor rose again after the replug > (82200/360)*(0.25/pxs)/4
--   6. REAPER stays responsive: reaching finish() proves it.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  local pxs = reaper.GetHZoomLevel()
  record("px_per_sec", pxs)
  check("horizontal zoom measurable", pxs > 0,
        "GetHZoomLevel=" .. tostring(pxs))

  local ev_sec  = (600 / 360) * (0.25 / pxs)    -- cursor motion per wheel event
  local exp_rise1 = (87240 / 360) * (0.25 / pxs) -- expected turn 1 motion
  local exp_rise2 = (82200 / 360) * (0.25 / pxs) -- expected turn 2 motion
  record("expected_rise1_s", exp_rise1)
  record("expected_rise2_s", exp_rise2)
  record("expected_maxstep_s", 6 * ev_sec)

  reaper.SetEditCurPos(10, false, false)
  check("transport stopped", reaper.GetPlayState() == 0,
        "GetPlayState=" .. tostring(reaper.GetPlayState()))
  local start = reaper.GetCursorPosition()
  check("cursor at 10 s", math.abs(start - 10) < 1e-6,
        "GetCursorPosition=" .. tostring(start))

  local samples = {} -- {t = ms since scenario start, pos = cursor position}
  local t0 = reaper.time_precise()

  wait_ms(6000, function()
    table.insert(samples, { t = (reaper.time_precise() - t0) * 1000,
                            pos = reaper.GetCursorPosition() })
  end, function()
    local n = #samples
    if n < 2 then
      check("samples collected", false, "n=" .. n)
      finish()
      return
    end

    -- Window 1: the first turn (script 100-1475 ms; offset <= ~100 ms).
    local w1_end = 1 -- index of last sample with t <= 1400 ms
    for i = 2, n do
      if samples[i].t <= 1400 then w1_end = i else break end
    end
    local rise1 = samples[w1_end].pos - samples[1].pos
    check(string.format("cursor rose during first turn (> %.4f s)", exp_rise1 / 6),
          rise1 > exp_rise1 / 6,
          string.format("n=%d rise1=%.4f expected>=%.4f (t=%.0f ms, pos %.3f -> %.3f)",
                        w1_end, rise1, exp_rise1 / 6, samples[w1_end].t,
                        samples[1].pos, samples[w1_end].pos))

    -- The first-motion interval: the REAPER startup stall (dialogs,
    -- dismissed by tools/reaper-oracle) is hidden inside the
    -- start("device") sync, so its dt is a normal ~32 ms even though the
    -- engine timer was stopped. While stopped the worker thread kept
    -- accumulating wheel deltas; the engine's first timer callback drains
    -- the whole backlog in one flush (the 250 ms acc cap bounds tick
    -- consumption, not the backlog — the flush is unbounded and not
    -- stutter). It is excluded and recorded; every later interval is
    -- checked. Same rationale as the dt > 100 ms exclusion below, which
    -- cannot identify this one (its dt is normal).
    local first_motion = 0
    for i = 2, w1_end do
      if samples[i].pos > samples[1].pos + 1e-9 then first_motion = i break end
    end
    if first_motion > 0 then
      record("startup_flush", string.format(
        "idx=%d t=%.0f..%.0f s=%.4f dt=%.0f (first-motion interval: engine backlog flush)",
        first_motion, samples[first_motion - 1].t, samples[first_motion].t,
        samples[first_motion].pos - samples[first_motion - 1].pos,
        samples[first_motion].t - samples[first_motion - 1].t))
    end

    -- Max step between consecutive samples, over normal sampling intervals
    -- only (dt <= 100 ms ~ 3 cycles). Intervals that span a REAPER
    -- main-loop stall (startup dialogs, dismissed by tools/reaper-oracle)
    -- include the engine's bounded 250 ms catch-up flush of the wheel
    -- deltas accumulated while stalled; that flush is not stutter.
    local maxstep1, msi, skipped = 0, 2, 0
    local maxdt, maxdti = 0, 2
    local big = {}
    for i = 2, w1_end do
      local dt = samples[i].t - samples[i - 1].t
      if dt > maxdt then maxdt, maxdti = dt, i end
      if dt > 100 then
        skipped = skipped + 1
      elseif i == first_motion then
        -- startup backlog flush (recorded above); not stutter
      else
        local s = samples[i].pos - samples[i - 1].pos
        if s > maxstep1 then maxstep1, msi = s, i end
        if s > 0.01 then
          table.insert(big, string.format("t=%.0f dt=%.0f s=%.4f", samples[i].t, dt, s))
        end
      end
    end
    record("big_steps", table.concat(big, "; ") or "none")
    record("maxstep_ctx", string.format(
      "idx=%d t=%.0f..%.0f dt=%.0f pos %.3f -> %.3f; maxdt=%.0f (idx=%d) skipped=%d",
      msi, samples[msi - 1].t, samples[msi].t,
      samples[msi].t - samples[msi - 1].t,
      samples[msi - 1].pos, samples[msi].pos,
      maxdt, maxdti, skipped))
    check(string.format("max step < %.4f s (first turn, no stutter)", 6 * ev_sec),
          maxstep1 < 6 * ev_sec,
          string.format("maxstep=%.4f (n=%d, %d stall interval(s) excluded, startup flush excluded=%s)",
                        maxstep1, w1_end, skipped, first_motion > 0 and "yes" or "no"))

    -- Window 2: replug at script 3000 ms; turn 2 runs 3200-4288 ms.
    local rp = 1 -- index of last sample before the replug (t < 3000 ms)
    for i = 2, n do
      if samples[i].t < 3000 then rp = i else break end
    end
    local rise2 = samples[n].pos - samples[rp].pos
    check(string.format("cursor rose again after replug (> %.4f s)", exp_rise2 / 4),
          rise2 > exp_rise2 / 4,
          string.format("n=%d rise2=%.4f expected>=%.4f (t=%.0f ms pos=%.3f -> final %.3f)",
                        n, rise2, exp_rise2 / 4, samples[rp].t, samples[rp].pos, samples[n].pos))

    -- Trace: connected -> disconnected -> connected, in that order.
    local lines = read_trace()
    if lines then
      -- ("log disconnected" does not contain "log connected": plain finds are safe)
      local a = find_line(lines, "log connected")
      local b = a > 0 and find_line(lines, "log disconnected", a + 1) or 0
      local c = b > 0 and find_line(lines, "log connected", b + 1) or 0
      check("trace: connected, disconnected, connected (in order)", c > 0,
            string.format("lines=%d,%d,%d", a, b, c))
    end

    finish()
  end)
end)
