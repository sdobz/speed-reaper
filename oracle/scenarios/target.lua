-- oracle/scenarios/target.lua — M5 targeting end-to-end
-- (vibes/31-m5-targeting.md task 6). Proves the M5 composite ids and their
-- REAPER-visible behavior: RIPL DEL as an implicit-target verb under ripple
-- off, undo, RIPL DEL on a time selection, TRIM IN with nothing selected,
-- and the TIMELINE selection verbs.
--
-- The harness sources oracle/scenarios/target.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/target.txt (device clock; the CLI
-- script starts ~40 ms after plugin load, so script time = device time -
-- ~40 ms, same offset the buttons scenario relies on):
--   400/450      RIPL DEL tap 0x2B (fires at release, ~410 script ms)
--                -> cmds 40310, 40006, 40309: A gone, B ripples 5 -> 0,
--                ripple mode back to off, selection restored (none)
--   1600-1800    ESC double-tap 0x31 (fires ~1760) -> cmd 40029 (undo):
--                A [0,5] + B [5,10] restored, nothing selected
--   2800/2850    IN tap 0x07 (deferred 300 ms, fires ~3110)
--                -> cmd 40625: tsel start -> cursor 1.0 (script moves it)
--   3400/3450    OUT tap 0x08 (deferred, fires ~3710)
--                -> cmd 40626: tsel end -> cursor 2.0 (script moves it)
--   4000/4050    RIPL DEL tap 0x2B (fires ~4010, tsel present)
--                -> cmd 40201: tsel [1,2] removed, gap closed:
--                A [0,5] -> [0,1], B 5 -> 4 (shifted by 1)
--   4600/4650    TRIM IN tap 0x09 (fires ~4610, nothing selected)
--                -> cmd 40511 inside WithTargetSelected: A left edge ->
--                cursor 0.5 (script moves it), selection restored (none)
--   5200/5250    TIMELINE tap 0x1B (deferred, fires ~5510)
--                -> A selected exclusively (project API, no cmd)
--   5900/5950    TIMELINE tap 0x1B (fires ~6210)
--                -> sole selection -> nothing selected
--   6600-6800    TIMELINE double-tap (fires ~6760) -> cmd 40289 (deselect
--                all): nothing selected
--
-- Setup: fresh instance = new project; one track with adjacent items
-- A [0,5] and B [5,10], NOTHING selected, edit cursor at 2.5 s, ripple off,
-- no time selection.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  reaper.Main_OnCommand(40309, 0) -- ripple off (guarantee the starting mode)
  reaper.Main_OnCommand(40635, 0) -- remove any time selection
  reaper.InsertTrackAtIndex(0, true) -- returns bool; track is index 0
  local track = reaper.GetTrack(nil, 0)
  local a = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
  local b = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(b, "D_POSITION", 5.0)
  reaper.SetMediaItemInfo_Value(b, "D_LENGTH", 5.0)
  reaper.SetMediaItemSelected(a, false)
  reaper.SetMediaItemSelected(b, false)
  reaper.SetEditCurPos(2.5, false, false)
  reaper.UpdateArrange()
  -- API-added items have no undo point: without this, undo rolls back to an empty project.
  reaper.Undo_OnStateChange("setup")

  local function snap()
    local n = reaper.CountMediaItems(nil, track)
    local p0, l0, p1, l1, s0, s1, p2 = -1, -1, -1, -1, -1, -1, -1
    for i = 0, n - 1 do
      local it = reaper.GetTrackMediaItem(track, i)
      local p = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      local l = reaper.GetMediaItemInfo_Value(it, "D_LENGTH")
      local sel = reaper.GetMediaItemInfo_Value(it, "B_UISEL") ~= 0 and 1 or 0
      if i == 0 then p0, l0, s0 = p, l, sel
      elseif i == 1 then p1, l1, s1 = p, l, sel
      elseif i == 2 then p2 = p end
    end
    local t0, t1 = reaper.GetSet_LoopTimeRange(false, false, 0.0, 0.0, false)
    return {
      n = n, p0 = p0, l0 = l0, p1 = p1, l1 = l1, p2 = p2,
      s0 = s0, s1 = s1,
      tsel0 = t0, tsel1 = t1,
      rip = reaper.GetToggleCommandState(40310) +
            reaper.GetToggleCommandState(40311),
    }
  end

  local function nitems()
    local n = reaper.CountMediaItems(nil, track)
    local minpos = nil
    for i = 0, n - 1 do
      local it = reaper.GetTrackMediaItem(track, i)
      local p = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      if minpos == nil or p < minpos then minpos = p end
    end
    return n, minpos
  end

  local c0, mp0 = nitems()
  check("setup: two items, first at pos 0", c0 == 2 and mp0 == 0.0,
        "count=" .. c0 .. " minpos=" .. tostring(mp0))
  check("setup: edit cursor at 2.5 s",
        math.abs(reaper.GetCursorPosition() - 2.5) < 1e-6,
        "GetCursorPosition=" .. tostring(reaper.GetCursorPosition()))
  local r0 = reaper.GetToggleCommandState(40310) +
             reaper.GetToggleCommandState(40311)
  check("setup: ripple off", r0 == 0, "ripple state=" .. tostring(r0))

  -- Sample every defer cycle for ~7.8 s (last effect: TIMELINE double-tap
  -- lands ~6760 ms in). Every effect holds until the next step that changes
  -- that state, so all windows are >= 300 ms.
  local samples = {}
  local t0 = reaper.time_precise()
  local moved1, moved2, moved3 = false, false, false

  wait_ms(7800, function()
    local st = (reaper.time_precise() - t0) * 1000
    -- Move the edit cursor for the IN / OUT / TRIM IN steps (each must be
    -- done before the step's command fires, ~3110 / ~3710 / ~4610 ms).
    if not moved1 and st > 2400 and st < 3000 then
      moved1 = true
      reaper.SetEditCurPos(1.0, false, false)
    end
    if not moved2 and st > 3150 and st < 3650 then
      moved2 = true
      reaper.SetEditCurPos(2.0, false, false)
    end
    if not moved3 and st > 4200 and st < 4500 then
      moved3 = true
      reaper.SetEditCurPos(0.5, false, false)
    end
    local s = snap()
    s.t = st
    table.insert(samples, s)
  end, function()
    local n = #samples
    if n < 2 then
      check("samples collected", false, "n=" .. n)
      finish()
      return
    end

    -- One mid-window sample per step (script clock; device - ~40 ms).
    local function mid(lo, hi)
      for i = 1, n do
        if samples[i].t >= lo and samples[i].t <= hi then return samples[i] end
      end
      return nil
    end

    -- RIPL DEL tap (~410 ms): A gone, B at 0 (gap closed although ripple
    -- was off — the composite switches it on around the delete), ripple
    -- back to off, nothing selected.
    local s1 = mid(500, 1500)
    check("RIPL DEL: A deleted (count 2 -> 1)", s1 and s1.n == 1,
          string.format("s1=%s n=%s", tostring(s1 and s1.t),
                        tostring(s1 and s1.n)))
    check("RIPL DEL: B moved to 0 (gap closed, ripple was off)",
          s1 and math.abs(s1.p0 - 0.0) < 1e-6 and math.abs(s1.l0 - 5.0) < 1e-6,
          string.format("p0=%.17g l0=%.17g", s1 and s1.p0 or -1,
                        s1 and s1.l0 or -1))
    check("RIPL DEL: ripple mode back to off", s1 and s1.rip == 0,
          string.format("rip=%s", tostring(s1 and s1.rip)))
    check("RIPL DEL: selection unchanged (none)", s1 and s1.s0 == 0,
          string.format("sel0=%s", tostring(s1 and s1.s0)))

    -- ESC double-tap / undo (~1760 ms): A and B restored.
    local s2 = mid(1900, 2700)
    check("undo: A [0,5] and B [5,10] restored",
          s2 and s2.n == 2 and math.abs(s2.p0) < 1e-6
              and math.abs(s2.l0 - 5.0) < 1e-6
              and math.abs(s2.p1 - 5.0) < 1e-6
              and math.abs(s2.l1 - 5.0) < 1e-6,
          string.format("n=%s p0=%s l0=%s p1=%s l1=%s",
                        tostring(s2 and s2.n), tostring(s2 and s2.p0),
                        tostring(s2 and s2.l0), tostring(s2 and s2.p1),
                        tostring(s2 and s2.l1)))
    check("undo: nothing selected",
          s2 and s2.s0 == 0 and s2.s1 == 0,
          string.format("sel0=%s sel1=%s",
                        tostring(s2 and s2.s0), tostring(s2 and s2.s1)))

    -- IN + OUT taps (~3110 / ~3710 ms): time selection [1,2] on A.
    local s3 = mid(3800, 4000)
    check("IN/OUT: time selection [1,2]",
          s3 and math.abs(s3.tsel0 - 1.0) < 0.05
              and math.abs(s3.tsel1 - 2.0) < 0.05,
          string.format("tsel0=%s tsel1=%s",
                        tostring(s3 and s3.tsel0),
                        tostring(s3 and s3.tsel1)))

    -- RIPL DEL on the time selection (~4010 ms): tsel removed, gap closed.
    -- [1,2] lies inside A [0,5], so A splits: A [0,1], A' [1,4] (was
    -- [2,5], shifted by 1), and B 5 -> 4 (shifted by 1).
    local s4 = mid(4100, 4550)
    check("RIPL DEL (tsel): time selection removed (zero-length range)",
          s4 and math.abs(s4.tsel1 - s4.tsel0) < 1e-6,
          string.format("tsel0=%s tsel1=%s",
                        tostring(s4 and s4.tsel0),
                        tostring(s4 and s4.tsel1)))
    check("RIPL DEL (tsel): A [0,1], A' [1,4], B shifted by 1 (pos 4)",
          s4 and s4.n == 3 and math.abs(s4.p0) < 1e-6
              and math.abs(s4.l0 - 1.0) < 1e-6
              and math.abs(s4.p1 - 1.0) < 1e-6
              and math.abs(s4.l1 - 3.0) < 1e-6
              and math.abs(s4.p2 - 4.0) < 1e-6,
          string.format("n=%s p0=%s l0=%s p1=%s l1=%s p2=%s",
                        tostring(s4 and s4.n), tostring(s4 and s4.p0),
                        tostring(s4 and s4.l0), tostring(s4 and s4.p1),
                        tostring(s4 and s4.l1), tostring(s4 and s4.p2)))

    -- TRIM IN tap (~4610 ms), nothing selected: A's left edge -> cursor 0.5,
    -- selection restored (none).
    local s5 = mid(4700, 5450)
    check("TRIM IN: A left edge at cursor (A [0.5,1])",
          s5 and math.abs(s5.p0 - 0.5) < 0.05
              and math.abs((s5.p0 + s5.l0) - 1.0) < 0.05,
          string.format("p0=%s l0=%s", tostring(s5 and s5.p0),
                        tostring(s5 and s5.l0)))
    check("TRIM IN: selection unchanged (none)",
          s5 and s5.s0 == 0 and s5.s1 == 0,
          string.format("sel0=%s sel1=%s",
                        tostring(s5 and s5.s0), tostring(s5 and s5.s1)))

    -- TIMELINE tap 1 (~5510 ms): A selected exclusively.
    local s6 = mid(5600, 6150)
    check("TIMELINE tap: A selected, B not",
          s6 and s6.s0 == 1 and s6.s1 == 0,
          string.format("sel0=%s sel1=%s",
                        tostring(s6 and s6.s0), tostring(s6 and s6.s1)))

    -- TIMELINE tap 2 (~6210 ms): sole selection -> nothing selected.
    local s7 = mid(6300, 6600)
    check("TIMELINE tap x2: nothing selected",
          s7 and s7.s0 == 0 and s7.s1 == 0,
          string.format("sel0=%s sel1=%s",
                        tostring(s7 and s7.s0), tostring(s7 and s7.s1)))

    -- TIMELINE double-tap (~6760 ms, cmd 40289): nothing selected.
    local s8 = mid(6850, 7700)
    check("TIMELINE x2: nothing selected",
          s8 and s8.s0 == 0 and s8.s1 == 0,
          string.format("sel0=%s sel1=%s",
                        tostring(s8 and s8.s0), tostring(s8 and s8.s1)))

    -- Trace checks (build/oracle/trace_target.txt, lines are "<ms> <msg>").
    local trace = os.getenv("ORACLE_OUT") .. "/trace_target.txt"
    local f = io.open(trace, "r")
    check("trace readable", f ~= nil, trace)
    if f then
      local lines = {}
      for line in f:lines() do table.insert(lines, line) end
      f:close()

      -- 1) The 9 gesture lines, in script order.
      local gest = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) ([a-z_]+)$")
        if k then table.insert(gest, { k, kind }) end
      end
      local want = { { "002b", "tap" }, { "0031", "double_tap" },
                     { "0007", "tap" }, { "0008", "tap" },
                     { "002b", "tap" }, { "0009", "tap" },
                     { "001b", "tap" }, { "001b", "tap" },
                     { "001b", "double_tap" } }
      local gest_ok = #gest == #want
      for i = 1, #want do
        if not (gest[i] and gest[i][1] == want[i][1]
                and gest[i][2] == want[i][2]) then gest_ok = false end
      end
      local gest_dump = ""
      for i = 1, math.min(#gest, 12) do
        gest_dump = gest_dump .. gest[i][1] .. "/" .. gest[i][2] .. " "
      end
      check("trace: 9 gestures in script order",
            gest_ok, string.format("n=%d [%s]", #gest, gest_dump))

      -- 2) The cmd lines, in order: the RIPL DEL composite (ripple off),
      -- undo, IN, OUT, RIPL DEL (time selection), TRIM IN, TIMELINE x2.
      local cmds = {}
      for _, l in ipairs(lines) do
        local c = l:match(" cmd (%d+)$")
        if c then table.insert(cmds, tonumber(c)) end
      end
      local want_cmds = { 40310, 40006, 40309, 40029, 40625, 40626, 40201,
                          40511, 40289 }
      local cmd_ok = #cmds == #want_cmds
      for i = 1, #want_cmds do
        if cmds[i] ~= want_cmds[i] then cmd_ok = false end
      end
      local cmd_dump = ""
      for _, c in ipairs(cmds) do cmd_dump = cmd_dump .. c .. " " end
      check("trace: cmd lines are the composite ids in order", cmd_ok,
            string.format("n=%d [%s]", #cmds, cmd_dump))

      -- 3) No `target none` — every T verb in this scenario had a target.
      local none = 0
      for _, l in ipairs(lines) do
        if l:find("target none", 1, true) then none = none + 1 end
      end
      check("trace: no `target none` (all T verbs had a target)", none == 0,
            "count=" .. none)
    end

    finish()
  end)
end)
