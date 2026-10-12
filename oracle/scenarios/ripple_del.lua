-- oracle/scenarios/ripple_del.lua — M3: what RIPL DEL (cmd 40006) actually
-- does under each REAPER ripple mode.
--
-- SMOKE-TEST.md "M3" step 4 is stateful by design: 40006 is
-- "Item: Remove items" — REAPER has no standalone "ripple delete" command,
-- so whether the gap closes after the delete is driven by the project's
-- ripple mode (off / per-track / all tracks; workflow doc: "respects
-- ripple state"). This scenario settles that behavior on this REAPER build:
--   ripple off (40309)        : item deleted, gap REMAINS  (B stays at 5)
--   ripple per-track (40310)  : item deleted, gap CLOSES   (B moves to 0)
--   ripple all-tracks (40311) : item deleted, gap CLOSES   (B moves to 0)
-- Items are adjacent (A [0,5], B [5,10]) so the closed-gap position is
-- unambiguous. No fake device: everything is driven from Lua.
dofile(os.getenv("ORACLE_LIB"))

run(function()
  record("app_version", reaper.GetAppVersion())

  reaper.InsertTrackAtIndex(0, true)
  local track = reaper.GetTrack(nil, 0)

  -- Adjacent A [0,5] (selected), B [5,10] (not selected), cursor inside A.
  local function reset_items()
    while reaper.CountMediaItems(nil, track) > 0 do
      reaper.DeleteTrackMediaItem(track, reaper.GetTrackMediaItem(track, 0))
    end
    local a = reaper.AddMediaItemToTrack(track)
    reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
    reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
    local b = reaper.AddMediaItemToTrack(track)
    reaper.SetMediaItemInfo_Value(b, "D_POSITION", 5.0)
    reaper.SetMediaItemInfo_Value(b, "D_LENGTH", 5.0)
    reaper.SetMediaItemSelected(a, true)
    reaper.SetMediaItemSelected(b, false)
    reaper.SetEditCurPos(2.5, false, false)
    reaper.UpdateArrange()
  end

  local function remaining()
    local n = reaper.CountMediaItems(nil, track)
    if n ~= 1 then return n, nil end
    return n, reaper.GetMediaItemInfo_Value(
      reaper.GetTrackMediaItem(track, 0), "D_POSITION")
  end

  -- Set ripple mode, rebuild items, fire RIPL DEL, assert, then continue.
  local function phase(name, set_cmd, want_pos, next_fn)
    reaper.Main_OnCommand(set_cmd, 0)
    reset_items()
    record(name .. ": ripple on/off state (1162)",
           reaper.GetToggleCommandState(1162))
    reaper.Main_OnCommand(40006, 0) -- "Item: Remove items" (RIPL DEL)
    wait_ms(400, nil, function()
      local n, pos = remaining()
      check(name .. ": selected item deleted (count 2 -> 1)", n == 1,
            "count=" .. tostring(n))
      check(name .. ": gap " ..
            (want_pos == 5.0 and "remains (B still at 5)"
                             or "closes (B moved to 0)"),
            n == 1 and pos ~= nil and math.abs(pos - want_pos) < 1e-6,
            "pos=" .. tostring(pos))
      if next_fn then next_fn() end
    end)
  end

  phase("ripple off", 40309, 5.0, function()
    phase("ripple per-track", 40310, 0.0, function()
      phase("ripple all-tracks", 40311, 0.0, function()
        reaper.Main_OnCommand(40309, 0) -- restore ripple off
        finish()
      end)
    end)
  end)
end)
