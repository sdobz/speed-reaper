-- probe_smth.lua — throwaway probe: what does 40916 actually do?
-- Config A: one track, items P [0,3] and Q [3,6] meeting at 3, time sel
-- [2,4] over the meeting point, both selected.
-- Config B: same, unselected.
-- Config C: two tracks, A [0,5] on t1, C [0,5] on t2 (full overlap),
-- time sel [1,2], nothing selected (the simple_verbs setup).
-- Records item fades / positions after Main_OnCommand(40916).
dofile(os.getenv("ORACLE_LIB"))

local function dump(tag, tracks, tsel0, tsel1)
  local parts = {}
  for t = 0, tracks - 1 do
    local tr = reaper.GetTrack(nil, t)
    local n = reaper.CountMediaItems(nil, tr)
    for i = 0, n - 1 do
      local it = reaper.GetTrackMediaItem(tr, i)
      if not it then break end
      local p = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      local l = reaper.GetMediaItemInfo_Value(it, "D_LENGTH")
      local fain = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN")
      local fain_a = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN_AUTO")
      local fout = reaper.GetMediaItemInfo_Value(it, "D_FADEOUTLEN")
      local fout_a = reaper.GetMediaItemInfo_Value(it, "D_FADEOUTLEN_AUTO")
      local xf = reaper.GetMediaItemInfo_Value(it, "D_CROSSFADELEN")
      local xfn = reaper.GetMediaItemInfo_Value(it, "D_CROSSFADENEXT")
      table.insert(parts, string.format(
        "%s t%d i%d [%g,%g] fin=%g finA=%g fout=%g foutA=%g xf=%g xfn=%g",
        tag, t, i, p, p + l, fain, fain_a, fout, fout_a, xf, xfn))
    end
  end
  record(tag .. "_items", table.concat(parts, " | "))
end

start("settled", function()
  -- Config S: the simple_verbs setup, dumped immediately (no device input).
  reaper.InsertTrackAtIndex(0, true)
  reaper.InsertTrackAtIndex(1, true)
  local sa = reaper.GetTrack(nil, 0)
  local sb = reaper.GetTrack(nil, 1)
  local ia = reaper.AddMediaItemToTrack(sa)
  reaper.SetMediaItemInfo_Value(ia, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(ia, "D_LENGTH", 5.0)
  reaper.AddTakeToMediaItem(ia, true)
  reaper.AddTakeToMediaItem(ia, true)
  local ib = reaper.AddMediaItemToTrack(sb)
  reaper.SetMediaItemInfo_Value(ib, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(ib, "D_LENGTH", 5.0)
  reaper.AddTakeToMediaItem(ib, true)
  reaper.UpdateArrange()
  record("S_tracks", reaper.CountTracks(nil))
  record("S_items_t0",
         tostring(reaper.CountMediaItems(nil, sa)))
  record("S_items_t1",
         tostring(reaper.CountMediaItems(nil, sb)))
  record("S_takes", reaper.CountTakes(ia) .. "/" .. reaper.CountTakes(ib))
  -- Where are the items on track 0 actually?
  local pos0 = {}
  for i = 0, 4 do
    local it = reaper.GetTrackMediaItem(sa, i)
    if it then
      table.insert(pos0, string.format("i%d=[%g,%g]",
        i, reaper.GetMediaItemInfo_Value(it, "D_POSITION"),
             reaper.GetMediaItemInfo_Value(it, "D_LENGTH")))
    end
  end
  record("S_t0_items", table.concat(pos0, " ") or "none")
  -- and does a third item land on the right track?
  local ix = reaper.AddMediaItemToTrack(sa)
  reaper.AddTakeToMediaItem(ix, true)
  reaper.SetMediaItemInfo_Value(ix, "D_POSITION", 9.0)
  reaper.SetMediaItemInfo_Value(ix, "D_LENGTH", 1.0)
  reaper.UpdateArrange()
  record("S_items_t0_2nd",
         tostring(reaper.CountMediaItems(nil, sa)))

  -- command label, if the API exists in this build
  if reaper.GetCommandLabel then
    record("label_40916", reaper.GetCommandLabel(40916))
    record("label_40346", reaper.GetCommandLabel(40346))
  else
    record("label_api", "GetCommandLabel nil")
  end

  -- Config A: meeting items, selected.
  reaper.InsertTrackAtIndex(0, true)
  local tr = reaper.GetTrack(nil, 0)
  local p = reaper.AddMediaItemToTrack(tr)
  reaper.AddTakeToMediaItem(p, true)
  reaper.SetMediaItemInfo_Value(p, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(p, "D_LENGTH", 3.0)
  local q = reaper.AddMediaItemToTrack(tr)
  reaper.AddTakeToMediaItem(q, true)
  reaper.SetMediaItemInfo_Value(q, "D_POSITION", 3.0)
  reaper.SetMediaItemInfo_Value(q, "D_LENGTH", 3.0)
  reaper.SetMediaItemSelected(p, true)
  reaper.SetMediaItemSelected(q, true)
  reaper.GetSet_LoopTimeRange(true, true, 2.0, 4.0, true)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  dump("A_sel", reaper.CountTracks(nil))

  -- Config B: same layout fresh (new project), unselected.
  reaper.Main_NewProject(1)
  reaper.InsertTrackAtIndex(0, true)
  local tr2 = reaper.GetTrack(nil, 0)
  local p2 = reaper.AddMediaItemToTrack(tr2)
  reaper.AddTakeToMediaItem(p2, true)
  reaper.SetMediaItemInfo_Value(p2, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(p2, "D_LENGTH", 3.0)
  local q2 = reaper.AddMediaItemToTrack(tr2)
  reaper.AddTakeToMediaItem(q2, true)
  reaper.SetMediaItemInfo_Value(q2, "D_POSITION", 3.0)
  reaper.SetMediaItemInfo_Value(q2, "D_LENGTH", 3.0)
  reaper.GetSet_LoopTimeRange(true, true, 2.0, 4.0, true)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  dump("B_unsel", reaper.CountTracks(nil))

  -- Config C: the simple_verbs setup (A/C overlapping on two tracks).
  reaper.Main_NewProject(1)
  reaper.InsertTrackAtIndex(0, true)
  reaper.InsertTrackAtIndex(1, true)
  local t1 = reaper.GetTrack(nil, 0)
  local t2 = reaper.GetTrack(nil, 1)
  local a = reaper.AddMediaItemToTrack(t1)
  reaper.AddTakeToMediaItem(a, true)
  reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
  local c = reaper.AddMediaItemToTrack(t2)
  reaper.AddTakeToMediaItem(c, true)
  reaper.SetMediaItemInfo_Value(c, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(c, "D_LENGTH", 5.0)
  reaper.GetSet_LoopTimeRange(true, true, 1.0, 2.0, true)
  reaper.UpdateArrange()
  record("C_before_total", tostring(reaper.CountMediaItems(nil)))
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  record("C_after_total", tostring(reaper.CountMediaItems(nil)))
  dump("C_overlap", reaper.CountTracks(nil))
  -- marker return-value order probe
  reaper.Main_OnCommand(40157, 0)
  local r1,r2,r3,r4,r5,r6,r7,r8 = reaper.EnumProjectMarkers(1)
  record("marker_ret", string.format("%s %s %s %s %s %s %s %s",
    tostring(r1),tostring(r2),tostring(r3),tostring(r4),tostring(r5),
    tostring(r6),tostring(r7),tostring(r8)))
  record("marker_count", tostring(reaper.EnumProjectMarkers(0)))

  check("probe done", true, "see info records")
  finish()
end)
