-- oracle/scenarios/verbs.lua — M3 edit verbs end-to-end (vibes/22-m3-edit-verbs.md
-- task 5). Validates the (key, kind) -> command-id table in
-- plugin/src/verbs.cpp against REAPER-visible effects.
--
-- The harness sources oracle/scenarios/verbs.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/verbs.txt (device clock; the CLI
-- script starts ~40 ms after plugin load, so script time = device time - ~40
-- ms, same offset the buttons scenario relies on):
--   400/450      SPLIT tap 0x2F            -> cmd 40012, item 1 -> 2
--   800/850      IN tap 0x07 (deferred:    -> cmd 40625, time sel start -> cursor
--                 promoted after 300 ms)
--   1200/1250    OUT tap 0x08 (deferred)   -> cmd 40626, time sel end -> cursor
--   1600/1650    TRIM IN tap 0x09          -> cmd 40511, item left edge -> cursor
--   2000-2200    IN double-tap 0x07        -> cmd 40635, time selection cleared
--   2400/2450    RIPL DEL tap 0x2B         -> cmd 40006, item count drops
--   2800/2850    SNAP tap 0x2E             -> cmd 1157, snap toggles, `led main=`
--   3200-3400    ESC double-tap 0x31       -> cmd 40029 (undo), no 40635 after it
--
-- Setup: fresh instance = new project; one empty item (pos 0, len 10 s,
-- selected) on a new track; edit cursor at 5 s. Every verb acts at the
-- cursor, so the cursor must stay put — none of the mapped verbs move it.
--
-- REAPER 7.81 quirk (verified in the oracle): SPLIT deselects both halves,
-- and 40511 (trim left) only trims *selected* items — so the scenario
-- re-selects the items just before the TRIM IN step for it to have a
-- target (the IN/OUT steps are time-selection commands and ignore item
-- selection, so this does not disturb their windows).
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  -- Setup: new project + one empty item + cursor at 5 s.
  reaper.InsertTrackAtIndex(0, true) -- returns bool; track is index 0
  local track = reaper.GetTrack(nil, 0)
  local item = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(item, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(item, "D_LENGTH", 10.0)
  reaper.SetMediaItemSelected(item, true)
  reaper.SetEditCurPos(5.0, false, false)
  reaper.UpdateArrange()

  local function items()
    local n = reaper.CountMediaItems(nil, track)
    local minpos = nil
    for i = 0, n - 1 do
      local it = reaper.GetTrackMediaItem(track, i)
      local p = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      if minpos == nil or p < minpos then minpos = p end
    end
    return n, minpos
  end


  local count0, minpos0 = items()
  check("setup: one item at pos 0", count0 == 1 and minpos0 == 0.0,
        "count=" .. count0 .. " minpos=" .. tostring(minpos0))
  check("setup: edit cursor at 5 s",
        math.abs(reaper.GetCursorPosition() - 5.0) < 1e-6,
        "GetCursorPosition=" .. tostring(reaper.GetCursorPosition()))

  -- Sample every defer cycle for ~4.5 s (last effect lands ~3.31 s in; the
  -- ESC check window runs to ~4.3 s). Every effect holds until the next step
  -- that changes that state, so all windows are >= 300 ms.
  local samples = {} -- {t, count, minpos, tsel0, tsel1, snap, undo}
  local t0 = reaper.time_precise()
  local reselected = false

  wait_ms(4500, function()
    local st = (reaper.time_precise() - t0) * 1000
    -- SPLIT (REAPER 7.81) deselects both halves; 40511 only trims
    -- *selected* items, so re-select them before the TRIM IN (~1610 ms)
    -- step (after the IN/OUT steps, which are time-selection commands and
    -- ignore item selection) for the trim step to have a target.
    if not reselected and st > 1200 and st < 1580 then
      reselected = true
      local n2 = reaper.CountMediaItems(nil, track)
      for i = 0, n2 - 1 do
        reaper.SetMediaItemSelected(reaper.GetTrackMediaItem(track, i), true)
      end
      reaper.UpdateArrange()
    end
    -- isSet, isLoop, startOut, endOut, allowautoseek (out args by value)
    local s0, s1 = reaper.GetSet_LoopTimeRange(false, false, 0.0, 0.0, false)
    local cnt, mp = items()
    table.insert(samples, {
      t = st,
      count = cnt,
      minpos = mp,
      tsel0 = s0, tsel1 = s1,
      snap = reaper.GetToggleCommandState(1157),
      undo = reaper.Undo_CanUndo2(nil),
    })
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
        if samples[i].t >= lo and samples[i].t <= hi then return i end
      end
      return 0
    end
    local function at(i, k) return samples[i] and samples[i][k] or nil end

    -- SPLIT (cmd 40012 ~410 ms): item count 1 -> 2, split at cursor 5.
    local i1 = mid(500, 1000)
    check("SPLIT: item count 1 -> 2", i1 ~= 0 and at(i1, "count") == 2,
          string.format("n=%d i1=%d count=%s", n, i1,
                        tostring(at(i1, "count"))))
    check("SPLIT: first half still starts at 0",
          i1 ~= 0 and at(i1, "minpos") ~= nil
              and math.abs(at(i1, "minpos") - 0.0) < 1e-6,
          string.format("minpos=%s", tostring(at(i1, "minpos"))))

    -- IN (cmd 40625 ~1110 ms): time selection start -> cursor 5.
    local i2 = mid(1200, 1450)
    check("IN: time sel start ~= 5",
          i2 ~= 0 and math.abs(at(i2, "tsel0") - 5.0) < 0.05,
          string.format("tsel0=%s tsel1=%s",
                        tostring(at(i2, "tsel0")), tostring(at(i2, "tsel1"))))

    -- OUT (cmd 40626 ~1510 ms): time selection end -> cursor 5.
    local i3 = mid(1650, 2000)
    check("OUT: time sel start ~= 5 and end ~= 5",
          i3 ~= 0 and math.abs(at(i3, "tsel0") - 5.0) < 0.05
              and math.abs(at(i3, "tsel1") - 5.0) < 0.05,
          string.format("tsel0=%s tsel1=%s",
                        tostring(at(i3, "tsel0")), tostring(at(i3, "tsel1"))))

    -- TRIM IN (cmd 40511 ~1610 ms): item left edge trimmed to cursor 5
    -- (the [0,5] half becomes zero-length and REAPER drops it).
    local i4 = mid(1700, 2000)
    check("TRIM IN: trimmed item edge at cursor (min pos ~= 5)",
          i4 ~= 0 and at(i4, "minpos") ~= nil
              and math.abs(at(i4, "minpos") - 5.0) < 0.05,
          string.format("count=%s minpos=%s",
                        tostring(at(i4, "count")),
                        tostring(at(i4, "minpos"))))

    -- IN double-tap (cmd 40635 ~2110 ms): time selection cleared ->
    -- zero-length range.
    local i5 = mid(2200, 2350)
    check("IN x2: time selection cleared (zero-length range)",
          i5 ~= 0 and math.abs(at(i5, "tsel0") - at(i5, "tsel1")) < 1e-6,
          string.format("tsel0=%s tsel1=%s",
                        tostring(at(i5, "tsel0")), tostring(at(i5, "tsel1"))))

    -- RIPL DEL (cmd 40006 ~2410 ms): item count drops.
    local c_before = at(i5, "count")
    local i6 = mid(2500, 2700)
    local c_after = at(i6, "count")
    check("RIPL DEL: item count drops",
          i5 ~= 0 and i6 ~= 0 and c_after < c_before,
          string.format("before=%s after=%s",
                        tostring(c_before), tostring(c_after)))

    -- SNAP (cmd 1157 ~2810 ms): snap toggle state flips.
    local i7b = mid(2550, 2750)
    local i7a = mid(2900, 3200)
    check("SNAP: GetToggleCommandState(1157) flips",
          i7b ~= 0 and i7a ~= 0
              and at(i7a, "snap") == 1 - at(i7b, "snap"),
          string.format("before=%s after=%s",
                        tostring(at(i7b, "snap")), tostring(at(i7a, "snap"))))
    local snap_after = at(i7a, "snap")

    -- ESC double-tap (cmd 40029 ~3310 ms, undo): the undo label
    -- (Undo_CanUndo2) changes once the undo lands.
    local u1 = at(i6, "undo")
    local i8 = mid(3450, 4300)
    local u2 = at(i8, "undo")
    check("ESC x2: Undo_CanUndo2 string changes after the undo",
          i8 ~= 0 and u1 ~= nil and u2 ~= nil and u1 ~= u2,
          string.format("before=%q after=%q",
                        tostring(u1), tostring(u2)))

    -- Trace checks (build/oracle/trace_verbs.txt, lines are "<ms> <msg>").
    local trace = os.getenv("ORACLE_OUT") .. "/trace_verbs.txt"
    local f = io.open(trace, "r")
    check("trace readable", f ~= nil, trace)
    if f then
      local lines = {}
      for line in f:lines() do table.insert(lines, line) end
      f:close()

      -- 1) The 8 gesture lines, in script order.
      local gest = {} -- {key = "002f", kind = "tap", ...}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) ([a-z_]+)$")
        if k then table.insert(gest, { k, kind }) end
      end
      local want = { { "002f", "tap" }, { "0007", "tap" },
                     { "0008", "tap" }, { "0009", "tap" },
                     { "0007", "double_tap" }, { "002b", "tap" },
                     { "002e", "tap" }, { "0031", "double_tap" } }
      local gest_ok = #gest == #want
      for i = 1, #want do
        if not (gest[i] and gest[i][1] == want[i][1]
                and gest[i][2] == want[i][2]) then gest_ok = false end
      end
      local gest_dump = ""
      for i = 1, math.min(#gest, 12) do
        gest_dump = gest_dump .. gest[i][1] .. "/" .. gest[i][2] .. " "
      end
      check("trace: 8 gestures in script order (taps + IN/ESC double-taps)",
            gest_ok, string.format("n=%d [%s]", #gest, gest_dump))

      -- 2) The cmd lines: exactly the 8 verb ids, in order.
      local cmds = {}
      for _, l in ipairs(lines) do
        local c = l:match(" cmd (%d+)$")
        if c then table.insert(cmds, tonumber(c)) end
      end
      local want_cmds = { 40012, 40625, 40626, 40511, 40635, 40006, 1157,
                          40029 }
      local cmd_ok = #cmds == #want_cmds
      for i = 1, #want_cmds do
        if cmds[i] ~= want_cmds[i] then cmd_ok = false end
      end
      local cmd_dump = ""
      for _, c in ipairs(cmds) do cmd_dump = cmd_dump .. c .. " " end
      check("trace: cmd lines are the 8 verb ids in script order", cmd_ok,
            string.format("n=%d [%s]", #cmds, cmd_dump))

      -- 3) `led main=` lines (plain; not the jog lines): present, final
      --    value matches the snap state (LED_SNAP = bit 5 = 0x20).
      local led_main, led_last = 0, nil
      for _, l in ipairs(lines) do
        local b = l:match(" led main=([0-9a-f]+)$")
        if b then
          led_main = led_main + 1
          led_last = tonumber(b, 16)
        end
      end
      local want_main = snap_after == 1 and 0x20 or 0x00
      check("trace: `led main=` traced, final value matches snap state",
            led_main >= 1 and led_last == want_main,
            string.format("lines=%d last=%s want=%d snap_after=%s",
                          led_main, tostring(led_last), want_main,
                          tostring(snap_after)))

      -- 4) The undo (cmd 40029) comes after the last time-selection clear
      --    (cmd 40635) — i.e. no 40635 follows the undo.
      local last_40635, last_40029 = 0, 0
      for i, l in ipairs(lines) do
        if l:find("cmd 40635", 1, true) then last_40635 = i end
        if l:find("cmd 40029", 1, true) then last_40029 = i end
      end
      check("trace: last cmd 40029 after last cmd 40635 (no clear after undo)",
            last_40029 > last_40635,
            string.format("line_40635=%d line_40029=%d",
                          last_40635, last_40029))
    end

    finish()
  end)
end)
