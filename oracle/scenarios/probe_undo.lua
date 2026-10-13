-- TEMP diagnostic (not part of the suite): why does REAPER hang / items
-- vanish after undo of the RIPL DEL composite block? Progress is appended
-- to $ORACLE_OUT/probe_progress.txt at every step.
dofile(os.getenv("ORACLE_LIB"))

local prog = io.open(os.getenv("ORACLE_OUT") .. "/probe_progress.txt", "w")
local function step(name)
  prog:write(name .. " @ " .. reaper.time_precise() .. "\n")
  prog:flush()
end

step("body start")

run(function()
  step("inside run")
  reaper.Main_OnCommand(40309, 0)
  step("ripple off")
  reaper.InsertTrackAtIndex(0, true)
  local track = reaper.GetTrack(nil, 0)
  local a = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
  local b = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(b, "D_POSITION", 5.0)
  reaper.SetMediaItemInfo_Value(b, "D_LENGTH", 5.0)
  reaper.SetEditCurPos(2.5, false, false)
  reaper.UpdateArrange()
  step("setup done")

  local function state()
    local n = reaper.CountMediaItems(nil, track)
    local s = "n=" .. n
    for i = 0, n - 1 do
      local it = reaper.GetTrackMediaItem(track, i)
      s = s .. string.format(" [%s:%s]",
        tostring(reaper.GetMediaItemInfo_Value(it, "D_POSITION")),
        tostring(reaper.GetMediaItemInfo_Value(it, "D_LENGTH")))
    end
    return s
  end

  reaper.Undo_BeginBlock()
  step("undo begin")
  reaper.SetMediaItemSelected(a, true)
  reaper.Main_OnCommand(40310, 0)
  step("ripple on")
  reaper.Main_OnCommand(40006, 0)
  step("delete")
  reaper.Main_OnCommand(40309, 0)
  step("ripple off")
  local n = reaper.CountMediaItems(nil, track)
  for i = 0, n - 1 do
    reaper.SetMediaItemSelected(reaper.GetTrackMediaItem(track, i), false)
  end
  reaper.Undo_EndBlock("Speed Reaper: delete", -1)
  step("undo end")

  wait_ms(400, nil, function()
    step("after block: " .. state())
    record("after block", state())
    reaper.Main_OnCommand(40029, 0)
    step("undo cmd sent")
    wait_ms(400, nil, function()
      step("after one undo: " .. state())
      record("after one undo", state())
      reaper.Main_OnCommand(40029, 0)
      wait_ms(400, nil, function()
        step("after two undos: " .. state())
        record("after two undos", state())
        record("undo desc", reaper.Undo_CanUndo2(nil))
        step("calling finish")
        finish()
        step("finish returned")
      end)
    end)
  end)
end)
step("run returned")
