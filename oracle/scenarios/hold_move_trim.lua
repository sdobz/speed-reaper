-- oracle/scenarios/hold_move_trim.lua — M7 hold+rotate: SPLIT move and
-- TRIM OUT holds (vibes/33-m7-hold-rotate.md task 5a). Validates the
-- HoldStart/HoldRotate/HoldEnd path in plugin/src/verbs.cpp against
-- REAPER-visible item geometry: one hold = one undo block, 4 wheel ticks
-- per step, one project frame (1/Fps()) per step, selection untouched.
--
-- The harness sources oracle/scenarios/hold_move_trim.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/hold_move_trim.txt (device
-- clock; the CLI script starts ~40 ms after plugin load, so script time
-- = device time - ~40 ms):
--   300/1200     SPLIT hold 0x2F (HoldStart ~650): +8 ticks at 700-724
--                -> A.pos 0 -> 2/30
--   1600/2500    TRIM OUT hold 0x0A (HoldStart ~1950): -8 ticks at
--                2000-2024 -> A.len 5 -> 5 - 2/30
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
    local pos, len, sel = -1, -1, 1
    if n == 1 then
      local it = reaper.GetTrackMediaItem(track, 0)
      pos = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      len = reaper.GetMediaItemInfo_Value(it, "D_LENGTH")
      sel = reaper.GetMediaItemInfo_Value(it, "B_UISEL") ~= 0 and 1 or 0
    end
    return n, pos, len, sel
  end

  local n0, p0, l0 = snap()
  check("setup: one item A at [0,5]", n0 == 1 and p0 == 0.0 and l0 == 5.0,
        string.format("n=%d pos=%s len=%s", n0, tostring(p0), tostring(l0)))
  check("setup: edit cursor at 2.5",
        math.abs(reaper.GetCursorPosition() - 2.5) < 1e-6,
        "GetCursorPosition=" .. tostring(reaper.GetCursorPosition()))

  -- Sample every defer cycle until the second hold has landed (last
  -- device event ~2500 ms; script clock is ~40 ms behind). The
  -- selection must stay empty through every sample.
  local bad_count, bad_sel, t0 = 0, 0, reaper.time_precise()
  local last
  wait_ms(3200, function()
    local n, pos, len, sel = snap()
    last = { t = (reaper.time_precise() - t0) * 1000, n = n,
             pos = pos, len = len }
    if n ~= 1 then bad_count = bad_count + 1 end
    if sel ~= 0 then bad_sel = bad_sel + 1 end
  end, function()
    local n, pos, len, sel = snap()
    local frame = 1.0 / 30.0
    local npos = 2.0 * frame
    local nlen = 5.0 - 2.0 * frame

    check("item count stays 1 throughout (no splits, no duplicates)",
          bad_count == 0,
          "samples_with_other_count=" .. bad_count)
    check("selection stays empty throughout both holds",
          bad_sel == 0 and sel == 0,
          "samples_with_selection=" .. bad_sel .. " final_sel=" .. sel)
    check("SPLIT hold +8 ticks: A.pos ~= +2/30",
          n == 1 and math.abs(pos - npos) < 0.001,
          string.format("pos=%s want=%s", tostring(pos), tostring(npos)))
    check("TRIM OUT hold -8 ticks: A.len ~= 5 - 2/30",
          n == 1 and math.abs(len - nlen) < 0.001,
          string.format("len=%s want=%s last_sample=%s",
                        tostring(len), tostring(nlen),
                        last and string.format(
                          "t=%.0f pos=%s len=%s", last.t,
                          tostring(last.pos), tostring(last.len))
                          or "none"))

    -- Trace: SPLIT hold (002f) then TRIM OUT hold (000a); each hold
    -- traces hold_start, >=1 hold_rotate (one per wheel event) and
    -- hold_end, in that order. Collapse consecutive repeats.
    local lines = read_trace()
    if lines then
      local kinds = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) (hold_[a-z]+)$")
        if k then table.insert(kinds, k .. " " .. kind) end
      end
      local collapsed = {}
      for _, kk in ipairs(kinds) do
        if collapsed[#collapsed] ~= kk then
          table.insert(collapsed, kk)
        end
      end
      local want = { "002f hold_start", "002f hold_rotate", "002f hold_end",
                     "000a hold_start", "000a hold_rotate",
                     "000a hold_end" }
      local ok = #collapsed == #want
      for i = 1, #want do
        if collapsed[i] ~= want[i] then ok = false end
      end
      check("trace: SPLIT hold then TRIM OUT hold (start/rotate/end each)",
            ok, "got [" .. table.concat(collapsed, " | ") .. "]")

      -- No command ran: hold verbs act via the project API, not RunCommand.
      local cmds = 0
      for _, l in ipairs(lines) do
        if l:match(" cmd %d+$") then cmds = cmds + 1 end
      end
      check("trace: no cmd lines (hold verbs use the project API)",
            cmds == 0, "cmd_lines=" .. cmds)
    end

    finish()
  end)
end)
