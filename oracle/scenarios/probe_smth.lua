-- probe_smth.lua — throwaway probe: what does 40916 actually do?
-- No Main_NewProject (not in this Lua build); separate tracks per config.
-- Config A: canonical (docs/reaper-plugin-workflow.md) — two MEETING items
--   P [0,3], Q [3,6] on ONE track, time selection [2,4] over the meeting.
-- Config B: task setup — two [0,5] items fully overlapping on two tracks,
--   time selection [1,2] (the simple_verbs setup).
-- Config C: same layout as B, fresh tracks — fallback id 41059.
-- Config D: same layout as A, fresh tracks — fallback id 41059.
-- Also probes: EnumProjectMarkers return order after 40157, and whether
-- GetSetMediaTrackInfo_String "NAME" sticks.
dofile(os.getenv("ORACLE_LIB"))

-- Count items on one track by loop (CountMediaItems(nil, tr) returns the
-- PROJECT total in this build).
local function track_items(tr)
  local its = {}
  for i = 0, 8 do
    local it = reaper.GetTrackMediaItem(tr, i)
    if not it then break end
    its[#its + 1] = it
  end
  return its
end

local function dump(tag, trs)
  local parts = {}
  for k, tr in ipairs(trs) do
    for _, it in ipairs(track_items(tr)) do
      local p = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      local l = reaper.GetMediaItemInfo_Value(it, "D_LENGTH")
      local fain = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN")
      local fain_a = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN_AUTO")
      local fout = reaper.GetMediaItemInfo_Value(it, "D_FADEOUTLEN")
      local fout_a = reaper.GetMediaItemInfo_Value(it, "D_FADEOUTLEN_AUTO")
      local xf = reaper.GetMediaItemInfo_Value(it, "D_CROSSFADELEN")
      local xfn = reaper.GetMediaItemInfo_Value(it, "D_CROSSFADENEXT")
      table.insert(parts, string.format(
        "%s tr%s [%g,%g] fin=%g finA=%g fout=%g foutA=%g xf=%g xfn=%g",
        tag, k, p, p + l, fain, fain_a, fout, fout_a, xf, xfn))
    end
  end
  record(tag .. "_items", table.concat(parts, " | ") or "(no items)")
end

-- Add two takes to an item (so crossfade has audio-ish content).
local function mkitem(tr, pos, len)
  local it = reaper.AddMediaItemToTrack(tr)
  reaper.AddTakeToMediaItem(it, true)
  reaper.AddTakeToMediaItem(it, true)
  reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos)
  reaper.SetMediaItemInfo_Value(it, "D_LENGTH", len)
  return it
end

local function set_cursor(pos)
  if reaper.GetSetEditCurPos then
    return reaper.GetSetEditCurPos(true, pos, false)
  elseif reaper.SetEditCurPos then
    return reaper.SetEditCurPos(pos, false, false) -- (dPos, bSeekEdit, bSeekTransport)
  elseif reaper.JumpToTime then
    return reaper.JumpToTime(true, pos)
  end
  record("cursor_api", "none available")
  return false
end

start("settled", function()
  local n = reaper.CountTracks(nil)
  for _ = 1, 8 do reaper.InsertTrackAtIndex(n, true) end
  local t0 = reaper.GetTrack(nil, 0)
  local t1 = reaper.GetTrack(nil, 1)
  local t2 = reaper.GetTrack(nil, 2)
  local t3 = reaper.GetTrack(nil, 3)
  local t4 = reaper.GetTrack(nil, 4)
  local t5 = reaper.GetTrack(nil, 5)
  local t6 = reaper.GetTrack(nil, 6)
  local t7 = reaper.GetTrack(nil, 7)

  -- Config A: meeting items on one track (t0), tsel [2,4], cmd 40916.
  mkitem(t0, 0.0, 3.0)
  mkitem(t0, 3.0, 3.0)
  reaper.GetSet_LoopTimeRange(true, true, 2.0, 4.0, true)
  reaper.UpdateArrange()
  dump("A_before", { t0 })
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  dump("A_after_40916", { t0 })

  -- Config B: overlapping [0,5] on two tracks (t1,t2), tsel [1,2], cmd 40916.
  mkitem(t1, 0.0, 5.0)
  mkitem(t2, 0.0, 5.0)
  reaper.GetSet_LoopTimeRange(true, true, 1.0, 2.0, true)
  reaper.UpdateArrange()
  dump("B_before", { t1, t2 })
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  dump("B_after_40916", { t1, t2 })

  -- Config C: same layout (t3,t4), tsel [1,2], fallback cmd 41059.
  mkitem(t3, 0.0, 5.0)
  mkitem(t4, 0.0, 5.0)
  reaper.GetSet_LoopTimeRange(true, true, 1.0, 2.0, true)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(41059, 0)
  reaper.UpdateArrange()
  dump("C_after_41059", { t3, t4 })

  -- Config D: meeting items on one track (t5), tsel [2,4], fallback 41059.
  mkitem(t5, 0.0, 3.0)
  mkitem(t5, 3.0, 3.0)
  reaper.GetSet_LoopTimeRange(true, true, 2.0, 4.0, true)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(41059, 0)
  reaper.UpdateArrange()
  dump("D_after_41059", { t5 })

  -- Config E: OVERLAPPING items on ONE track (t6): e1 [0,3.5], e2 [1.5,5],
  -- overlap [1.5,3.5] = tsel, cmd 40916.
  mkitem(t6, 0.0, 3.5)
  mkitem(t6, 1.5, 3.5)
  reaper.GetSet_LoopTimeRange(true, true, 1.5, 3.5, true)
  reaper.UpdateArrange()
  dump("E_before", { t6 })
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  dump("E_after_40916", { t6 })

  -- Config F: same overlapping pair (t7), tsel [1.5,3.5], fallback 41059.
  mkitem(t7, 0.0, 3.5)
  mkitem(t7, 1.5, 3.5)
  reaper.GetSet_LoopTimeRange(true, true, 1.5, 3.5, true)
  reaper.UpdateArrange()
  dump("F_before", { t7 })
  reaper.Main_OnCommand(41059, 0)
  reaper.UpdateArrange()
  dump("F_after_41059", { t7 })

  -- Marker return-order probe: cursor at 1.0, insert marker (40157).
  local cs = set_cursor(1.0)
  record("cursor_set", cs ~= nil and tostring(cs) or "nil")
  local ep = reaper.GetCursorPosition and reaper.GetCursorPosition() or -1
  record("cursor_pos", tostring(ep))
  reaper.Main_OnCommand(40157, 0)
  local cnt = reaper.EnumProjectMarkers(0)
  record("marker_count", tostring(cnt))
  local r1, r2, r3, r4, r5, r6, r7, r8 = reaper.EnumProjectMarkers(1)
  local function tv(v) return v ~= nil and tostring(v) or "nil" end
  record("marker_ret1",
         table.concat({ tv(r1), tv(r2), tv(r3), tv(r4),
                        tv(r5), tv(r6), tv(r7), tv(r8) }, "|"))

  -- Track-name stickiness probe.
  local nm = "NAMETEST"
  local ok1 = reaper.GetSetMediaTrackInfo_String(t0, "NAME", nm, #nm + 1, true)
  local buf, l2 = reaper.GetSetMediaTrackInfo_String(t0, "NAME", "", 0, false)
  record("name_set_len1",
         (ok1 ~= nil and tostring(ok1) or "nil") ..
         " read=" .. (buf ~= nil and tostring(buf) or "nil") ..
         " len=" .. (l2 ~= nil and tostring(l2) or "nil"))
  -- attempt 6 used buf_len=1 on the read; redo exactly that.
  local buf3, l3 = reaper.GetSetMediaTrackInfo_String(t0, "NAME", "x", 1, false)
  record("name_read_len1",
         "buf=" .. (buf3 ~= nil and tostring(buf3) or "nil") ..
         " len=" .. (l3 ~= nil and tostring(l3) or "nil"))
  -- set with buf_len=1 (attempt 6's set) on t1, then read.
  reaper.GetSetMediaTrackInfo_String(t1, "NAME", "B", 1, true)
  local buf4, l4 = reaper.GetSetMediaTrackInfo_String(t1, "NAME", "", 0, false)
  record("name_setB_len1",
         "buf=" .. (buf4 ~= nil and tostring(buf4) or "nil") ..
         " len=" .. (l4 ~= nil and tostring(l4) or "nil"))

  check("probe done", true, "see info records")
  finish()
end)
