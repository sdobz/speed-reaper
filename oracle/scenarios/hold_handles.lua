-- oracle/scenarios/hold_handles.lua — the new `nudge` turn handles
-- (vibes/41-action-registry.md task 7): fade_in, fade_out, roll, sel_in,
-- sel_out, each with step=100ms (2 steps = 0.2 s for 8 wheel ticks).
-- The script writes a bindings file (TRANS_DUR/DIS/ROLL/IN/OUT .turn) and
-- runs "Speed Reaper: reload bindings"; the fake device then holds each key
-- and turns the wheel (oracle/fake/hold_handles.txt, device time = script
-- time + ~40 ms):
--   300   TRANS_DUR +8 ticks -> A.fadein  0 -> 0.2
--   1000  DIS       +8 ticks -> A.fadeout 0 -> 0.2
--   1700  ROLL      +8 ticks -> A [0,4.2], B [4.2,10]
--   2400  IN        +8 ticks -> time selection start 1 -> 1.2
--   3100  OUT       -8 ticks -> time selection end   3 -> 2.8
-- Setup: A [0,4], B [4,10] on one track, nothing selected, cursor at 2
-- (A is the implicit cursor target), time selection [1,3].
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  reaper.InsertTrackAtIndex(0, true)
  local track = reaper.GetTrack(nil, 0)
  local function add(pos, len)
    local it = reaper.AddMediaItemToTrack(track)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos)
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", len)
    reaper.SetMediaItemSelected(it, false)
    -- REAPER's new-item default is a 10 ms fade: start from none
    reaper.SetMediaItemInfo_Value(it, "D_FADEINLEN", 0.0)
    reaper.SetMediaItemInfo_Value(it, "D_FADEOUTLEN", 0.0)
    return it
  end
  local a, b = add(0.0, 4.0), add(4.0, 6.0)
  reaper.SetEditCurPos(2.0, false, false)
  reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 3.0, false)
  reaper.UpdateArrange()
  reaper.Undo_OnStateChange("setup")

  local path = reaper.GetResourcePath() .. "/speed-reaper/bindings.ini"
  local f = io.open(path, "w")
  check("setup: bindings.ini writable", f ~= nil, path)
  if not f then return finish() end
  f:write("[bindings]\n",
          "TRANS_DUR.turn = nudge fade_in step=100ms\n",
          "DIS.turn = nudge fade_out step=100ms\n",
          "ROLL.turn = nudge roll step=100ms\n",
          "IN.turn = nudge sel_in step=100ms\n",
          "OUT.turn = nudge sel_out step=100ms\n")
  f:close()
  local reload = reaper.NamedCommandLookup("_SPEED_REAPER_RELOAD_BINDINGS")
  check("reload action registered", reload > 0, "id=" .. tostring(reload))
  reaper.Main_OnCommand(reload, 0)

  local function num(it, k) return reaper.GetMediaItemInfo_Value(it, k) end
  local function near(x, y) return math.abs(x - y) < 0.01 end

  local bad_sel = 0
  wait_ms(4000, function()
    if num(a, "B_UISEL") ~= 0 or num(b, "B_UISEL") ~= 0 then bad_sel = bad_sel + 1 end
  end, function()
    check("fade_in: A.fadein ~= 0.2", near(num(a, "D_FADEINLEN"), 0.2),
          "fadein=" .. num(a, "D_FADEINLEN"))
    check("fade_out: A.fadeout ~= 0.2", near(num(a, "D_FADEOUTLEN"), 0.2),
          "fadeout=" .. num(a, "D_FADEOUTLEN"))
    check("fade_in/out: B untouched",
          near(num(b, "D_FADEINLEN"), 0) and near(num(b, "D_FADEOUTLEN"), 0),
          "b.fadein=" .. num(b, "D_FADEINLEN"))
    check("roll: A.len ~= 4.2 (pos unchanged)",
          near(num(a, "D_LENGTH"), 4.2) and near(num(a, "D_POSITION"), 0.0),
          string.format("a.pos=%s a.len=%s", num(a, "D_POSITION"), num(a, "D_LENGTH")))
    check("roll: B.pos ~= 4.2, B.len ~= 5.8 (total length fixed)",
          near(num(b, "D_POSITION"), 4.2) and near(num(b, "D_LENGTH"), 5.8),
          string.format("b.pos=%s b.len=%s", num(b, "D_POSITION"), num(b, "D_LENGTH")))
    local s0, s1 = reaper.GetSet_LoopTimeRange2(nil, false, false, 0.0, 0.0, false)
    check("sel_in: time selection start ~= 1.2", near(s0, 1.2),
          string.format("start=%s end=%s", tostring(s0), tostring(s1)))
    check("sel_out: time selection end ~= 2.8", near(s1, 2.8),
          string.format("start=%s end=%s", tostring(s0), tostring(s1)))
    check("selection stays empty throughout", bad_sel == 0, "samples=" .. bad_sel)

    local lines = read_trace()
    if lines then
      local cmds = 0
      for _, l in ipairs(lines) do if l:match(" cmd %d+$") then cmds = cmds + 1 end end
      check("trace: no cmd lines (turn verbs use the project API)", cmds == 0,
            "cmd_lines=" .. cmds)
    end
    os.remove(path)  -- later scenarios/runs get the default file again
    finish()
  end)
end)
