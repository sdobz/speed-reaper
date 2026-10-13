-- oracle/scenarios/hid_wheel.lua — fake Speed Editor device end-to-end
-- (vibes/20-m1-wheel-playhead.md task 7).
--
-- The harness sources oracle/scenarios/hid_wheel.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/wheel_unplug.txt. The plugin's
-- HID worker opens the scripted device at plugin load and plays:
--   100–1200 ms    first wheel turn (137 x `wheel 600` ~ 1.1 s of cursor)
--   1471 ms        last decaying delta of turn 1
--   1500 ms        unplug   -> worker logs `disconnected`
--   3000 ms        plug     -> worker reconnects on its 1 s open retry
--   3200–4196 ms   second wheel turn
-- The CLI script starts ~40 ms after plugin load (measured in the oracle),
-- so the sample clock below tracks the script clock with a small offset.
--
-- Checks:
--   1. cursor rose > 0.2 s during the first turn (sample window t <= 1400 ms)
--   2. max step between consecutive samples in that window < 0.05 s (stutter)
--   3. trace has `log connected`, then `log disconnected`, then
--      `log connected` again, in that order
--   4. cursor rose again after the replug (before ~3000 ms vs final)
--   5. REAPER stays responsive: reaching finish() proves it.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

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

    -- Window 1: the first turn (script 100-1471 ms; offset <= ~100 ms).
    local w1_end = 1 -- index of last sample with t <= 1400 ms
    for i = 2, n do
      if samples[i].t <= 1400 then w1_end = i else break end
    end
    local rise1 = samples[w1_end].pos - samples[1].pos
    check("cursor rose during first turn (> 0.2 s)", rise1 > 0.2,
          string.format("n=%d rise1=%.4f (t=%.0f ms, pos %.3f -> %.3f)",
                        w1_end, rise1, samples[w1_end].t,
                        samples[1].pos, samples[w1_end].pos))

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
    check("max step < 0.05 s (first turn, no stutter)", maxstep1 < 0.05,
          string.format("maxstep=%.4f (n=%d, %d stall interval(s) excluded)",
                        maxstep1, w1_end, skipped))

    -- Window 2: replug at script 3000 ms; turn 2 runs 3200-4196 ms.
    local rp = 1 -- index of last sample before the replug (t < 3000 ms)
    for i = 2, n do
      if samples[i].t < 3000 then rp = i else break end
    end
    local rise2 = samples[n].pos - samples[rp].pos
    check("cursor rose again after replug", rise2 > 0.05,
          string.format("n=%d rise2=%.4f (t=%.0f ms pos=%.3f -> final %.3f)",
                        n, rise2, samples[rp].t, samples[rp].pos, samples[n].pos))

    -- Trace: connected -> disconnected -> connected, in that order.
    local trace = os.getenv("ORACLE_OUT") .. "/trace_hid_wheel.txt"
    -- state 0 = want `log connected`, 1 = want `log disconnected`,
    -- 2 = want `log connected` again, 3 = done. (Note: "log disconnected"
    -- does not contain "log connected", so plain substring finds are safe.)
    local state, detail = 0, "trace file missing: " .. trace
    local f = io.open(trace, "r")
    if f then
      for line in f:lines() do
        if state == 0 and line:find("log connected", 1, true) then
          state = 1
        elseif state == 1 and line:find("log disconnected", 1, true) then
          state = 2
        elseif state == 2 and line:find("log connected", 1, true) then
          state = 3
          break
        end
      end
      f:close()
      detail = string.format("connected=%d disconnected=%d reconnected=%d",
                             state >= 1 and 1 or 0, state >= 2 and 1 or 0,
                             state >= 3 and 1 or 0)
    end
    check("trace: connected, disconnected, connected (in order)", state >= 3,
          detail)

    finish()
  end)
end)
