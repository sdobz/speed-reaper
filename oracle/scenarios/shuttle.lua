-- oracle/scenarios/shuttle.lua — SHTL (shuttle) end-to-end under the oracle
-- (vibes/23-m4-shuttle.md task 6; approach: docs/archive/plugin-design-m4-shuttle-impl.md,
-- rule 2 — forward via playrate, reverse via stepped seek).
--
-- The harness sources oracle/scenarios/shuttle.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/shuttle.txt. The scripted device
-- plays on a clock starting at plugin load; the scenario syncs its clock on
-- the plugin's `log connected` trace line (no fixed warm-up timer), so
-- lua t ≈ script t. Windows below leave margin for it:
--   300/350      SHTL tap        -> mode Shtl (protocol wheel mode 3).
--                                   NOTE: while in Shtl the absolute offset
--                                   is still 0 -> dead band -> HardStop,
--                                   so the transport is PAUSED right after
--                                   the press (the documented law; the
--                                   wheel must be out of the band to play).
--   ~520         (lua ~480)      scenario starts play (OnPlayButtonEx),
--   700          wheel +4096     -> ShuttleRate 8.0; REAPER's master
--   (lua 660+)                      playrate clamps to 0.25..4.0, so the
--                                   read-back (Master_GetPlayRate) is 4.0,
--                                   not 8.0 — the documented cap; the
--                                   playhead must advance > 2x realtime
--   1700         wheel 0         -> dead band -> HardStop: paused, rate 1.0
--   ~1860        (lua ~1900)     scenario resumes play (OnPlayButtonEx) so
--   2300         wheel -2068     the stepped reverse has a playing transport
--   (lua 2260+)                    -> ShuttleRate -4.5: seek back 4.5x per
--                                   tick vs 1.0x transport forward -> net
--                                   ~3.5x backward; playhead must decrease
--   3400/3450    JOG tap while   -> hand-off: rate 1.0, mode back to Jog
--   3900/3950    STOP/PLAY (now -> cmd 40044 (toggle) -> stopped
--                mode Jog)
-- Transport control is OnPlayButtonEx, never 40045 (stop command blocks the
-- script/defer queue headless — M4 task 3).
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  reaper.SetEditCurPos(10, false, false)
  check("transport stopped", reaper.GetPlayState() == 0,
        "GetPlayState=" .. tostring(reaper.GetPlayState()))
  local start = reaper.GetCursorPosition()
  check("cursor at 10 s", math.abs(start - 10) < 1e-6,
        "GetCursorPosition=" .. tostring(start))

  local samples = {} -- {t = lua ms, pos = playhead, state, rate}
  local t0 = reaper.time_precise()
  local phase = 0 -- 0 = before play-start, 1 = before post-pause resume

  wait_ms(4500, function()
    local t = (reaper.time_precise() - t0) * 1000
    -- Transport start/resume (headless-safe OnPlayButtonEx, never 40045).
    -- At script ~520 ms: the SHTL press's dead-band HardStop (offset still
    -- 0) has just paused the transport; start play before the +4096 event
    -- (script 700) so the forward rate has a playing transport to apply to.
    -- At script ~1860 ms: resume after the wheel-0 dead-band pause, before
    -- the -2068 event (script 2300) so the stepped reverse runs.
    if phase == 0 and t >= 480 then
      phase = 1
      reaper.OnPlayButtonEx(0)
    elseif phase == 1 and t >= 1900 then
      phase = 2
      reaper.OnPlayButtonEx(0)
    end
    table.insert(samples, { t = t,
                            pos = reaper.GetPlayPositionEx(0),
                            state = reaper.GetPlayState(),
                            rate = reaper.Master_GetPlayRate(0) })
  end, function()
    local n = #samples
    if n < 2 then
      check("samples collected", false, "n=" .. n)
      finish()
      return
    end
    -- Forward (script 700-1700 -> lua ~660-1660): playhead advances well
    -- above realtime and the read-back rate is REAPER's documented 4.0
    -- cap of the requested 8.0 (M4 spike: master playrate clamps 0.25..4.0;
    -- docs/archive/plugin-design-m4-shuttle-impl.md).
    local fa, fb = window(samples, 800, 1450)
    check("forward window sampled", fa ~= nil and fb ~= nil and fb > fa,
          string.format("fa=%s fb=%s n=%d", tostring(fa), tostring(fb), n))
    if fa then
      local rise = samples[fb].pos - samples[fa].pos
      local r = samples[fb].rate
      check("forward: playhead advanced > 2x realtime (offset +4096 -> rate 8.0 requested)",
            rise / math.max((samples[fb].t - samples[fa].t) / 1000, 1e-3) > 2.0,
            string.format("rise=%.3f over t=%.0f..%.0f (%.4f -> %.4f) read-back=%.3f",
                          rise, samples[fa].t, samples[fb].t,
                          samples[fa].pos, samples[fb].pos, r))
      check("forward: Master_GetPlayRate 4.0 (requested 8.0; REAPER clamps to 0.25..4.0)",
            math.abs(r - 4.0) < 0.05,
            string.format("read=%.4f", r))
    end

    -- Dead band (script 1700 -> lua ~1660; resume at lua 1900): paused,
    -- rate back to 1.0, playhead held.
    local pa, pb = window(samples, 1720, 1840)
    check("pause window sampled", pa ~= nil and pb ~= nil and pb > pa,
          string.format("pa=%s pb=%s", tostring(pa), tostring(pb)))
    if pa then
      local stopped, rate_ok, dev = true, true, 0
      for i = pa, pb do
        if samples[i].state ~= 0 then stopped = false end
        if math.abs(samples[i].rate - 1.0) > 0.001 then rate_ok = false end
        local d = math.abs(samples[i].pos - samples[pa].pos)
        if d > dev then dev = d end
      end
      check("dead band (wheel 0): paused, rate 1.0, playhead held",
            stopped and rate_ok and dev < 0.02,
            string.format("t=%.0f..%.0f stopped=%s rate_ok=%s dev=%.4f",
                          samples[pa].t, samples[pb].t,
                          tostring(stopped), tostring(rate_ok), dev))
    end

    -- Reverse (script 2300 -> lua ~2260; resume was lua ~1900): the stepped
    -- seek moves the playhead back 4.5x per tick while the transport runs
    -- it forward 1.0x -> net ~3.5x backward.
    local ra, rb = window(samples, 2340, 3000)
    check("reverse window sampled", ra ~= nil and rb ~= nil and rb > ra,
          string.format("ra=%s rb=%s", tostring(ra), tostring(rb)))
    if ra then
      local fall = samples[rb].pos - samples[ra].pos
      local playing = samples[ra].state == 1
      check("reverse: playhead decreased (offset -2068 -> rate -4.5, stepped seek)",
            fall < -0.5,
            string.format("fall=%.3f over t=%.0f..%.0f (%.4f -> %.4f) playing=%s",
                          fall, samples[ra].t, samples[rb].t,
                          samples[ra].pos, samples[rb].pos, tostring(playing)))
    end

    -- JOG-tap hand-off (script 3400 -> lua ~3360): rate 1.0 and the mode
    -- is back to Jog (wheel relative again).
    local ja, jb = window(samples, 3440, 3800)
    check("jog hand-off window sampled", ja ~= nil and jb ~= nil and jb > ja,
          string.format("ja=%s jb=%s", tostring(ja), tostring(jb)))
    if ja then
      local r = samples[jb].rate
      check("JOG tap while shuttling: rate back to 1.0",
            math.abs(r - 1.0) < 0.05,
            string.format("read=%.4f (t=%.0f)", r, samples[jb].t))
    end

    -- STOP/PLAY (script 3900 -> lua ~3860): stopped at the end.
    local sa, sb = window(samples, 3980, 4500)
    check("stopped window sampled", sa ~= nil and sb ~= nil and sb > sa,
          string.format("sa=%s sb=%s", tostring(sa), tostring(sb)))
    if sa then
      local stopped = true
      for i = sa, sb do
        if samples[i].state ~= 0 then stopped = false end
      end
      check("STOP/PLAY while shuttling path: stopped (cmd 40044 toggle)",
            stopped,
            string.format("final GetPlayState=%d", samples[n].state))
    end

    -- Trace (trace_shuttle.txt, lines are "<ms> <msg>").
    local lines = read_trace()
    if not lines then finish() return end
    local idx = function(needle) return find_line(lines, needle) end
    local shtl_i = idx("mode Shtl")
    -- leading space keeps this away from `...Shtl`/`mode Jog` substring traps
    local jog_after_shtl = find_line(lines, " mode Jog", shtl_i + 1)
    local cmd40044 = 0
    for i = 1, #lines do
      if lines[i]:find("cmd 40044", 1, true) then cmd40044 = cmd40044 + 1 end
    end

    check("trace: mode Shtl entered and left (JOG tap hands off to Jog)",
          shtl_i > 0 and jog_after_shtl > 0,
          string.format("mode_shtl_line=%d mode_jog_after=%d",
                        shtl_i, jog_after_shtl))
    check("trace: SHTL LED on (led jog=2)", idx("led jog=2 main=0") > 0,
          "no `led jog=2` line")
    check("trace: forward rate applied once (shuttle rate=4.000 read=4.000)",
          idx("shuttle rate=4.000 read=4.000") > 0,
          "no `shuttle rate=` line: " .. tostring(lines[1] or "<empty>"))
    check("trace: reverse stepped seek ran (shuttle reverse=-4.500)",
          idx("shuttle reverse=-4.500 pos=") > 0,
          "no `shuttle reverse=` line")
    check("trace: cmd 40044 exactly once (STOP/PLAY, outside Shtl)",
          cmd40044 == 1, "count=" .. cmd40044)
    check("trace: device connected", idx("log connected") > 0,
          "no `log connected` line")

    finish()
  end)
end)
