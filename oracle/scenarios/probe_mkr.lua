-- probe_mkr.lua — throwaway follow-up to probe_smth.lua: settle
-- (a) EnumProjectMarkers return order for an in-range index (marker at 1.0),
-- (b) GetSetMediaTrackInfo_String "NAME" with the documented 4-arg Lua
--     binding (no buf_len) — does the name stick?
-- probe_smth.lua called EnumProjectMarkers(1) with only one marker (idx 0
-- is the count) -> out-of-range zeros; and passed 5 args to
-- GetSetMediaTrackInfo_String (buf_len shifted setNewValue). This probes
-- both with correct calls.
dofile(os.getenv("ORACLE_LIB"))

local function tv(v) return v ~= nil and tostring(v) or "nil" end

start("settled", function()
  -- (b) track name: 4-arg binding per reaper.fm docs (REAPER 7.82):
  --   boolean retval, string = GetSetMediaTrackInfo_String(tr, parm, str, set)
  -- REAPER 7.x param is "P_NAME" ("NAME" is the old/5.x spelling).
  reaper.InsertTrackAtIndex(0, true)
  local tr = reaper.GetTrack(nil, 0)
  local ok1 = reaper.GetSetMediaTrackInfo_String(tr, "P_NAME", "NAMETEST", true)
  local ok2, buf2 = reaper.GetSetMediaTrackInfo_String(tr, "P_NAME", "", false)
  record("name_pname", "set=" .. tv(ok1) .. " read=" .. tv(ok2) .. " buf=[" .. tv(buf2) .. "]")
  -- "NAME" param (what attempt 6 / probe_smth used) on a fresh track.
  reaper.InsertTrackAtIndex(1, false)
  local tr2 = reaper.GetTrack(nil, 1)
  local ok3 = reaper.GetSetMediaTrackInfo_String(tr2, "NAME", "FIVE", true)
  local ok4, buf4 = reaper.GetSetMediaTrackInfo_String(tr2, "NAME", "", false)
  record("name_name", "set=" .. tv(ok3) .. " read=" .. tv(ok4) .. " buf=[" .. tv(buf4) .. "]")
  -- cross-check: read tr (P_NAME-set) with "NAME" param, and tr2 with "P_NAME".
  local ok5, buf5 = reaper.GetSetMediaTrackInfo_String(tr, "NAME", "", false)
  local ok6, buf6 = reaper.GetSetMediaTrackInfo_String(tr2, "P_NAME", "", false)
  record("name_cross", "P_NAMEset_readNAME=[" .. tv(buf5) .. "] NAMEset_readP_NAME=[" .. tv(buf6) .. "]")

  -- (a) marker: put one project marker exactly at 1.0.
  local added = reaper.AddProjectMarker(nil, false, 1.0, 0.0, "M1", -1)
  local a0, b0, c0, d0, e0, f0 = reaper.EnumProjectMarkers(0) -- ALL returns, in-range idx 0
  local a1, b1, c1, d1, e1, f1, g1, h1 = reaper.EnumProjectMarkers(1)
  record("mkr_add", "added=" .. tv(added))
  record("mkr_e0", table.concat({ tv(a0), tv(b0), tv(c0), tv(d0),
                                  tv(e0), tv(f0) }, "|"))
  record("mkr_e1", table.concat({ tv(a1), tv(b1), tv(c1), tv(d1),
                                  tv(e1), tv(f1), tv(g1), tv(h1) }, "|"))
  -- recommended API cross-check.
  local nrm = reaper.GetNumRegionsOrMarkers and reaper.GetNumRegionsOrMarkers(nil)
  record("mkr_numrm", tv(nrm))
  if reaper.GetRegionOrMarker then
    local mk = reaper.GetRegionOrMarker(nil, 0, "")
    local pos = mk ~= nil and reaper.GetRegionOrMarkerInfo_Value(nil, mk, "D_STARTPOS")
    local isrgn = mk ~= nil and reaper.GetRegionOrMarkerInfo_Value(nil, mk, "B_ISREGION")
    local nm = mk ~= nil and reaper.GetSetRegionOrMarkerInfo_String(nil, mk, "NAME", "", false)
    record("mkr_gorm0", "pos=" .. tv(pos) .. " isrgn=" .. tv(isrgn) .. " name=[" .. tv(nm) .. "]")
  end
  -- cursor sanity for the record (SetEditCurPos is in this build).
  local sc = reaper.SetEditCurPos and reaper.SetEditCurPos(1.0, false, false)
  local cp = reaper.GetCursorPosition()
  record("cursor", "set=" .. tv(sc) .. " pos=" .. tv(cp))

  check("probe done", true, "see info records")
  finish()
end)
