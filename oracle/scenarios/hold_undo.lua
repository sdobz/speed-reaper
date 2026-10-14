-- oracle/scenarios/hold_undo.lua — M7 hold+rotate: a SPLIT hold followed
-- by an ESC double-tap (vibes/33-m7-hold-rotate.md task 5c). Validates
-- that the hold's 8 wheel-tick SetGeom writes land in ONE undo block
-- (Undo_Begin at HoldStart, Undo_End at HoldEnd — plugin/src/verbs.cpp),
-- so a single Edit: Undo (ESC double-tap, cmd 40029) restores the item
-- to its original position in one undo step, not eight.
--
-- The harness sources oracle/scenarios/hold_undo.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/hold_undo.txt (device
-- clock; the CLI script starts ~40 ms after plugin load, so script
-- time = device time - ~40 ms):
--   300/1200     SPLIT hold 0x2F (HoldStart ~650): +8 ticks at 700-724
--                -> A.pos 0 -> 2/30, one undo block "Speed Reaper: move"
--   1600-1800    ESC double-tap 0x31 (fires ~1750) -> cmd 40029 (undo)
--                -> A.pos 2/30 -> 0
--
-- Setup: fresh instance = new project; one track with item A [0,5],
-- NOTHING selected, edit cursor at 2.5 s (A is the implicit cursor
-- target, target.cpp rule 2), project 30 fps.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  -- Pin the project frame rate to 30 fps (REAPER's default; the pcall
  -- is a no-op if the key name is unknown on this build).
  pcall(reaper.GetSet_ProjectInfo, nil, "R_FRAME_RATE", 30.0)

  reaper.InsertTrackAtIndex(0, true) -- returns bool; track is index 0
  local track = reaper.GetTrack(nil, 0)
  local a = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
  reaper.SetMediaItemSelected(a, false)
  reaper.SetEditCurPos(2.5, false, false)
  reaper.UpdateArrange()
  -- API-added items have no undo point: without this, undo rolls back to
  -- an empty project.
  reaper.Undo_OnStateChange("setup")

  local function snap()
    local n = reaper.CountMediaItems(nil, track)
    local pos, sel = -1, 1
    if n == 1 then
      local it = reaper.GetTrackMediaItem(track, 0)
      pos = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      sel = reaper.GetMediaItemInfo_Value(it, "B_UISEL") ~= 0 and 1 or 0
    end
    local label = reaper.Undo_CanUndo2(nil)
    return n, pos, sel, label
  end

  local n0, p0, s0, label0 = snap()
  check("setup: one item A at [0,5], nothing selected",
        n0 == 1 and p0 == 0.0 and s0 == 0,
        string.format("n=%d pos=%s sel=%d", n0, tostring(p0), s0))
  check("setup: undo label is 'setup'",
        label0 == "setup", "label=" .. tostring(label0))

  -- Sample every defer cycle until the undo has landed (last device
  -- event ~1800 ms; script clock is ~40 ms behind). The selection must
  -- stay empty through every sample.
  local t0 = reaper.time_precise()
  local samples, bad_count, bad_sel = {}, 0, 0
  local function mid(lo, hi)
    for i = 1, #samples do
      if samples[i].t >= lo and samples[i].t <= hi then return i end
    end
    return 0
  end
  local function at(i, k) return samples[i] and samples[i][k] or nil end

  wait_ms(2400, function()
    local n, pos, sel, label = snap()
    table.insert(samples, { t = (reaper.time_precise() - t0) * 1000,
                            n = n, pos = pos, sel = sel,
                            label = label })
    if n ~= 1 then bad_count = bad_count + 1 end
    if sel ~= 0 then bad_sel = bad_sel + 1 end
  end, function()
    check("item count stays 1 throughout (no splits, no duplicates)",
          bad_count == 0,
          "samples_with_other_count=" .. bad_count)
    check("selection stays empty throughout",
          bad_sel == 0, "samples_with_selection=" .. bad_sel)

    -- Between HoldEnd (device ~1200) and the ESC double-tap (device
    -- ~1750): the hold has landed, the undo is still pending.
    local i1 = mid(1250, 1650)
    check("mid: A.pos ~= +2/30 before the undo",
          i1 ~= 0 and math.abs(at(i1, "pos") - 2.0 / 30.0) < 0.001,
          i1 ~= 0 and ("pos=" .. tostring(at(i1, "pos")))
                or "no sample in window")
    check("mid: undo label changed from 'setup' (the hold made an undo point)",
          i1 ~= 0 and at(i1, "label") ~= nil
              and at(i1, "label") ~= label0,
          i1 ~= 0 and ("label=" .. tostring(at(i1, "label")))
                or "no sample in window")
    record("mid_undo_label", i1 ~= 0 and at(i1, "label") or "n/a")

    -- Final: the single ESC double-tap undo restored A to its original
    -- position in ONE step. Eight separate undo blocks would leave
    -- pos at 1/30 and the undo label still "Speed Reaper: move".
    local last = samples[#samples]
    check("one undo step: A.pos restored to 0 (not eight steps)",
          last ~= nil and math.abs(last.pos - 0.0) < 0.001,
          "final_pos=" .. tostring(last and last.pos))
    check("one undo step: undo label back to 'setup'",
          last ~= nil and last.label == label0,
          "final_label=" .. tostring(last and last.label))

    -- Trace: the SPLIT hold (002f hold_start/hold_rotate/hold_end),
    -- then the ESC double-tap (0031), and exactly one cmd line: the
    -- undo (40029). Collapse consecutive gesture repeats.
    local lines = read_trace()
    if lines then
      local kinds = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) ([a-z_]+)$")
        if k then table.insert(kinds, k .. " " .. kind) end
      end
      local collapsed = {}
      for _, kk in ipairs(kinds) do
        if collapsed[#collapsed] ~= kk then
          table.insert(collapsed, kk)
        end
      end
      local want = { "002f hold_start", "002f hold_rotate",
                     "002f hold_end", "0031 double_tap" }
      local ok = #collapsed == #want
      for i = 1, #want do
        if collapsed[i] ~= want[i] then ok = false end
      end
      check("trace: SPLIT hold (start/rotate/end) then ESC double-tap",
            ok, "got [" .. table.concat(collapsed, " | ") .. "]")

      local cmds = {}
      for _, l in ipairs(lines) do
        local c = l:match(" cmd %d+$") and l:match(" cmd (%d+)$") or nil
        if c then table.insert(cmds, c) end
      end
      check("trace: exactly one cmd, the undo (40029)",
            #cmds == 1 and cmds[1] == "40029",
            "cmds=[" .. table.concat(cmds, ", ") .. "]")
    end

    finish()
  end)
end)
