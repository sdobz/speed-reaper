-- oracle/scenarios/bindings.lua — bindings.ini + the reload action
-- (vibes/41-action-registry.md task 5). The script writes a bindings file
-- binding SYNC_BIN.tap = action 40157 and CLOSE_UP.tap = targeted 40006
-- (plus one bad line), runs "Speed Reaper: reload bindings", then the fake
-- device presses SYNC_BIN (marker +1), CLOSE_UP with an UNSELECTED item
-- under the cursor (item removed, selection unchanged) and APPND (unbound:
-- no `cmd` trace).
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  reaper.InsertTrackAtIndex(0, true)
  local track = reaper.GetTrack(nil, 0)
  local item = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(item, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(item, "D_LENGTH", 10.0)
  reaper.SetMediaItemSelected(item, false)
  reaper.SetEditCurPos(5.0, false, false)
  reaper.UpdateArrange()

  local function markers()
    local _, nm, nr = reaper.CountProjectMarkers(nil)
    return nm + nr
  end
  local function selected_count()
    local n = 0
    for i = 0, reaper.CountMediaItems(nil) - 1 do
      if reaper.GetMediaItemInfo_Value(reaper.GetMediaItem(nil, i), "B_UISEL") ~= 0 then n = n + 1 end
    end
    return n
  end

  local path = reaper.GetResourcePath() .. "/speed-reaper/bindings.ini"
  local f = io.open(path, "w")
  check("setup: bindings.ini writable", f ~= nil, path)
  if not f then return finish() end
  f:write("[bindings]\n",
          "SYNC_BIN.tap = action 40157\n",
          "CLOSE_UP.tap = targeted 40006\n",
          "FOO.tap = action 1\n")
  f:close()
  local reload = reaper.NamedCommandLookup("_SPEED_REAPER_RELOAD_BINDINGS")
  check("reload action registered", reload > 0, "id=" .. tostring(reload))
  reaper.Main_OnCommand(reload, 0)

  local m0, items0 = markers(), reaper.CountMediaItems(nil)
  check("setup: no markers, one item, nothing selected",
        m0 == 0 and items0 == 1 and selected_count() == 0,
        string.format("markers=%d items=%d sel=%d", m0, items0, selected_count()))

  wait_ms(2000, nil, function()
    local m1, items1 = markers(), reaper.CountMediaItems(nil)
    check("SYNC_BIN: marker +1", m1 == m0 + 1, string.format("before=%d after=%d", m0, m1))
    check("CLOSE_UP: item removed", items1 == 0, "items=" .. items1)
    check("CLOSE_UP: selection unchanged (none)", selected_count() == 0,
          "sel=" .. selected_count())

    local lines = read_trace()
    if lines then
      local cmds, dump = {}, ""
      for _, l in ipairs(lines) do
        local c = l:match(" cmd (%d+)$")
        if c then table.insert(cmds, tonumber(c)); dump = dump .. c .. " " end
      end
      check("trace: cmds are exactly 40157, 40006 (APPND unbound)",
            #cmds == 2 and cmds[1] == 40157 and cmds[2] == 40006,
            "[" .. dump .. "]")
      check("trace: bad line reported with its line number",
            find_line(lines, "bindings.ini:4:") > 0)
      check("trace: bindings loaded n=2", find_line(lines, "bindings loaded n=2") > 0)
    end
    os.remove(path)  -- later scenarios/runs get the default file again
    finish()
  end)
end)
