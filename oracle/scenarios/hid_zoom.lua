-- oracle/scenarios/hid_zoom.lua — hold TIMELINE + wheel: arrange view zooms
-- around the edit cursor (vibes/46-jog-zoom.md task 6; math in
-- plugin/src/zoom.h, docs/jog-discovery.md §4.2).
--
-- The harness sources oracle/scenarios/hid_zoom.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/hid_zoom.txt. The scripted device
-- plays:
--   300–650 ms     TIMELINE (0x1B) held; HoldStart after the 350 ms hold
--   700–1172 ms    60 x `wheel 600`  = 36000 units = 100 detents CW  (zoom in)
--   1200–1672 ms   60 x `wheel -600` = 100 detents CCW               (zoom out)
--   1700 ms        TIMELINE up -> HoldEnd (no tap)
-- The CLI script starts ~40 ms after plugin load (see hid_wheel.lua), so
-- the sample clock tracks the script clock with a small offset.
--
-- Per detent the span is multiplied by (1 ∓ f), f = k_px / (pxPerSec · W)
-- with k_px = wheel.pixels_per_detent; over 100 detents the 10 s view
-- shrinks by a few percent (CW) then grows back past that minimum (CCW).
--
-- Checks:
--   1. pxPerSec > 0 (measurable headless zoom)
--   2. cursor at 5 s and the view spans 0–10 s before the turn
--   3. min span < 0.99 x 10 s: CW wheel shrinks the view
--   4. the cursor's screen fraction q = (cursor - L)/(R - L) is unchanged
--      within 2/(pxs * minspan) over all samples (zoom pivots on the edit
--      cursor). REAPER quantises GetSet_ArrangeView2 writes to the pixel
--      grid, so each edge can be off by up to 1 px = 1/pxs s; ±1 px on each
--      edge bounds the q drift by 2/(pxs * span). At ~100 px/s and a ~9.6 s
--      span that is ~2e-3.
--   5. max span after the CCW turn starts > min CW span x 1.01 (CCW grows)
--   6. REAPER stays responsive: reaching finish() proves it.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  local pxs = reaper.GetHZoomLevel()
  record("px_per_sec", pxs)
  check("horizontal zoom measurable", pxs > 0,
        "GetHZoomLevel=" .. tostring(pxs))

  reaper.SetEditCurPos(5, false, false)
  check("transport stopped", reaper.GetPlayState() == 0,
        "GetPlayState=" .. tostring(reaper.GetPlayState()))
  local cur = reaper.GetCursorPosition()
  check("cursor at 5 s", math.abs(cur - 5) < 1e-6,
        "GetCursorPosition=" .. tostring(cur))
  reaper.GetSet_ArrangeView2(nil, true, 0, 0, 0, 10)
  local L0, R0 = reaper.GetSet_ArrangeView2(nil, false, 0, 0, 0, 0)
  check("view spans 0-10 s", math.abs(L0) < 1e-9 and math.abs(R0 - 10) < 1e-9,
        string.format("L=%.6f R=%.6f", L0, R0))

  local samples = {} -- {t = ms since scenario start, L, R, q}
  local t0 = reaper.time_precise()

  wait_ms(2500, function()
    local L, R = reaper.GetSet_ArrangeView2(nil, false, 0, 0, 0, 0)
    if L == nil or R == nil or R <= L then return end -- skip bad read
    local span = R - L
    local q = (reaper.GetCursorPosition() - L) / span
    table.insert(samples, { t = (reaper.time_precise() - t0) * 1000,
                            L = L, R = R, q = q })
  end, function()
    local n = #samples
    if n < 2 then
      check("samples collected", false, "n=" .. n)
      finish()
      return
    end

    -- Min span: the CW turn (script 700-1172 ms). The CCW turn starts at
    -- script 1200 ms; the sample clock runs ~40 ms behind, so samples
    -- with t >= 1200 may still carry CW spans — harmless for a minimum.
    local minspan, mini = 1e30, 1
    for i = 1, n do
      local s = samples[i].R - samples[i].L
      if s < minspan then minspan, mini = s, i end
    end
    check("min span < 9.9 s (CW shrinks the view)", minspan < 9.9,
          string.format("minspan=%.6f (idx=%d t=%.0f L=%.4f R=%.4f)",
                        minspan, mini, samples[mini].t,
                        samples[mini].L, samples[mini].R))

    -- q invariant: the cursor's screen fraction must stay constant (the
    -- zoom pivots on the edit cursor). REAPER snaps each written view edge
    -- to the pixel grid (up to 1 px = 1/pxs s per edge), so the tolerance
    -- is ±1 px on each edge: 2/(pxs * minspan) on q.
    local tol = 2 / (pxs * minspan)
    local qmin, qmax = samples[1].q, samples[1].q
    for i = 1, n do
      if samples[i].q < qmin then qmin = samples[i].q end
      if samples[i].q > qmax then qmax = samples[i].q end
    end
    record("q_first", samples[1].q)
    record("q_tol", tol)
    record("q_range", string.format("min=%.6f max=%.6f", qmin, qmax))
    check(string.format("cursor screen fraction q unchanged within %.1e (2 px)", tol),
          qmax - qmin <= tol,
          string.format("q_first=%.6f min=%.6f max=%.6f tol=%.1e (n=%d)",
                        samples[1].q, qmin, qmax, tol, n))

    -- Max span after the CCW turn starts (script 1200 ms): the view must
    -- grow back past 1.01 x the CW minimum.
    local maxspan, maxi = 0, 1
    for i = 1, n do
      if samples[i].t >= 1200 then
        local s = samples[i].R - samples[i].L
        if s > maxspan then maxspan, maxi = s, i end
      end
    end
    record("spans", string.format(
      "min=%.6f (t=%.0f) max_after_ccw=%.6f (t=%.0f, n=%d)",
      minspan, samples[mini].t, maxspan, samples[maxi].t, n))
    check(string.format("max span after CCW > %.4f s (1.01 x min)",
                        minspan * 1.01),
          maxspan > minspan * 1.01,
          string.format("maxspan=%.6f min=%.6f (max at t=%.0f)",
                        maxspan, minspan, samples[maxi].t))

    finish()
  end)
end)
