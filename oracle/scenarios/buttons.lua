-- oracle/scenarios/buttons.lua — buttons + wheel-mode routing end-to-end
-- (vibes/21-m2-buttons-modes.md task 4).
--
-- The harness sources oracle/scenarios/buttons.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/buttons.txt:
--   300/350      JOG tap (mode already Jog: no state change)
--   600-912      wheel turn 1 (Jog): cursor must rise
--   1200/1250    SCRL tap -> mode Scrl
--   1500-1812    wheel turn 2 (Scrl): scrolls the view, cursor must NOT move
--   2100/2150    SHTL tap (ignored: mode stays Scrl, no trace)
--   2400/2450    STOP/PLAY tap -> cmd 40044, playback starts
--   2700         unplug -> `log disconnected`
--   3600         plug -> worker reconnects on its 1 s open retry and
--                re-sends the current mode (Scrl) + LED
--   4800-5112    wheel turn 3: must still be Scrl -> cursor must NOT move
-- The CLI script starts ~40 ms after plugin load (measured in the oracle),
-- so the sample clock below tracks the script clock with a small offset.
dofile(os.getenv("ORACLE_LIB"))

run(function()
  record("app_version", reaper.GetAppVersion())

  reaper.SetEditCurPos(10, false, false)
  check("transport stopped", reaper.GetPlayState() == 0,
        "GetPlayState=" .. tostring(reaper.GetPlayState()))
  local start = reaper.GetCursorPosition()
  check("cursor at 10 s", math.abs(start - 10) < 1e-6,
        "GetCursorPosition=" .. tostring(start))

  local samples = {} -- {t = ms since scenario start, pos, play}
  local t0 = reaper.time_precise()

  wait_ms(8000, function()
    table.insert(samples, { t = (reaper.time_precise() - t0) * 1000,
                            pos = reaper.GetCursorPosition(),
                            play = reaper.GetPlayState() })
  end, function()
    local n = #samples
    if n < 2 then
      check("samples collected", false, "n=" .. n)
      finish()
      return
    end

    -- Window 1: turn 1 (Jog) is script 600-912 ms; offset <= ~100 ms.
    local w1 = 1
    for i = 2, n do
      if samples[i].t <= 1100 then w1 = i else break end
    end
    local rise1 = samples[w1].pos - samples[1].pos
    check("cursor rose during JOG turn (> 0.1 s)", rise1 > 0.1,
          string.format("n=%d rise1=%.4f (t=%.0f ms, pos %.3f -> %.3f)",
                        w1, rise1, samples[w1].t,
                        samples[1].pos, samples[w1].pos))

    -- Window 2: turn 2 (Scrl) is script 1500-1812 ms -> samples 1450-1950.
    -- Cursor must be unchanged (+-0.001 s) throughout.
    local s2, e2 = 1, 1
    for i = 1, n do
      if samples[i].t >= 1450 then s2 = i; break end
    end
    for i = n, 1, -1 do
      if samples[i].t <= 1950 then e2 = i; break end
    end
    local dev2 = 0
    for i = s2, e2 do
      local d = math.abs(samples[i].pos - samples[s2].pos)
      if d > dev2 then dev2 = d end
    end
    check("cursor unchanged (+-0.001 s) during SCRL turn",
          e2 > s2 and dev2 <= 0.001,
          string.format("n=%d s2=%d e2=%d dev2=%.4f (t=%.0f ms)",
                        n, s2, e2, dev2, samples[e2].t))

    -- Window 3: after the replug (plug 3600 ms, reconnect <= ~4600 ms);
    -- turn 3 runs 4800-5112 ms. If the mode were lost (Jog), the cursor
    -- would rise ~0.3 s. Everything from t >= 4700 to the end must stay.
    local s3 = 1
    for i = 1, n do
      if samples[i].t >= 4700 then s3 = i; break end
    end
    local dev3 = 0
    for i = s3, n do
      local d = math.abs(samples[i].pos - samples[s3].pos)
      if d > dev3 then dev3 = d end
    end
    check("cursor unchanged (+-0.001 s) after replug (still Scrl)",
          n > s3 and dev3 <= 0.001,
          string.format("n=%d s3=%d dev3=%.4f (t=%.0f ms)",
                        n, s3, dev3, samples[s3].t))

    check("GetPlayState flipped (STOP/PLAY cmd 40044)",
          samples[n].play == 1,
          "final GetPlayState=" .. tostring(samples[n].play))

    -- Trace checks (trace_<scenario>.txt, lines are "<ms> <msg>").
    local trace = os.getenv("ORACLE_OUT") .. "/trace_buttons.txt"
    local f = io.open(trace, "r")
    check("trace readable", f ~= nil, trace)
    if not f then finish() return end
    local lines = {}
    for line in f:lines() do table.insert(lines, line) end
    f:close()

    local seen_jog, seen_scrl, shtl = false, false, false
    local order_bad, led_bad = false, false
    local led_total, led_first = 0, "?"
    local cur = "Jog"
    local want_bit = { Jog = "1", Scrl = "4", Shtl = "2" } -- one LED per mode
    local dis_i, cmd_count, scrl_led_after_disc = 0, 0, false
    for i = 1, #lines do
      local l = lines[i]
      local m = l:match(" mode ([A-Za-z]+)$")
      if m then
        if m == "Shtl" then shtl = true end
        if m == "Scrl" and not seen_jog then order_bad = true end
        seen_jog = seen_jog or (m == "Jog")
        seen_scrl = seen_scrl or (m == "Scrl")
        cur = m -- a `led` line follows its `mode` line
      end
      if l:find("log disconnected", 1, true) and dis_i == 0 then
        dis_i = i
      end
      local b = l:match(" led jog=([0-9a-f]+) main=([0-9a-f]+)$")
      if b then
        led_total = led_total + 1
        if led_first == "?" then led_first = b end
        if b ~= want_bit[cur] then led_bad = true end
        if dis_i > 0 and i > dis_i and b == "4" then
          scrl_led_after_disc = true
        end
      end
      if l:find("cmd 40044", 1, true) then cmd_count = cmd_count + 1 end
    end

    check("trace: mode Jog before mode Scrl",
          seen_jog and seen_scrl and not order_bad,
          string.format("jog=%d scrl=%d order_bad=%d",
                        seen_jog and 1 or 0, seen_scrl and 1 or 0,
                        order_bad and 1 or 0))
    check("trace: no mode Shtl", not shtl,
          shtl and "found `mode Shtl`" or "none")
    check("trace: led jog= lines single-LED and match the mode",
          led_total >= 3 and not led_bad and led_first == "1",
          string.format("led_lines=%d first=%s bad=%d",
                        led_total, led_first, led_bad and 1 or 0))
    check("trace: cmd 40044 exactly once", cmd_count == 1,
          "count=" .. cmd_count)
    check("trace: led jog=4 re-traced after replug",
          dis_i > 0 and scrl_led_after_disc,
          string.format("disconnected_at_line=%d scrl_led_after=%d",
                        dis_i, scrl_led_after_disc and 1 or 0))

    finish()
  end)
end)
