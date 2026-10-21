-- oracle/scenarios/xfade_keys.lua — the crossfade actions
-- (vibes/41-action-registry.md task 8; effects recorded in
-- docs/notes/crossfade-actions.md). Milestone 43 moved the default CUT/DIS
-- bindings to the ripple toggles (40310/40311), so this scenario writes a
-- scenario-local bindings.ini binding the crossfade ids (40916, 41193) —
-- and TRANS/TRANS_DUR, unchanged — to the same keys (model: bindings.lua).
-- Uses the scenario-local bindings file.
-- Four independent item pairs (one track each). Selection is re-armed
-- EVENT-DRIVEN, not on script clock: a REAPER startup stall (late-dialog
-- dismiss) shifts script time vs device time, and clock-based arming can
-- then land after a press. Each step's setup runs as soon as the previous
-- press's `cmd <id>` line appears in the plugin trace (flushed per line,
-- device time), long before the next press. Presses at 800/1200/1600/
-- 2000 ms (fake/xfade_keys.txt):
--   800  DIS       (40916)  C [0,5] + D [5,10] abut, time sel [4.5,5.5]:
--                           C and D overlap afterwards (C.len 5.5, D.pos 4.5)
--   1200 TRANS     (41059)  A [0,5] + B [4,9] overlap: crossfade flagged
--                           (A fade-out auto / B fade-in auto = 1.0)
--   1600 CUT       (41193)  G/H with manual fades 0.4: fades removed
--   2000 TRANS_DUR (43776)  I [0,5] + J [4,9] with auto crossfade: overlap
--                           shrinks to the default length (~0.02 s)
dofile(os.getenv("ORACLE_LIB"))

-- Crossfades need audio takes (empty items ignore them): a short tone WAV.
local function le16(x) return string.char(x % 256, math.floor(x / 256) % 256) end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end
local WAV = reaper.GetResourcePath():gsub("/$", "") .. "/xfade_keys_tone.wav"
local function write_tone(path)
  local f = io.open(path, "wb")
  if not f then return false end
  local sr, ns = 8000, 8000 * 8
  f:write("RIFF" .. le32(36 + ns * 2) .. "WAVEfmt " .. le32(16) .. le16(1) .. le16(1)
          .. le32(sr) .. le32(sr * 2) .. le16(2) .. le16(16) .. "data" .. le32(ns * 2))
  for k = 0, ns - 1 do
    f:write(le16(math.floor(math.sin(2 * math.pi * 440 * k / sr) * 16000) + 32768))
  end
  f:close()
  return true
end

start("device", function()
  record("app_version", reaper.GetAppVersion())

  -- Scenario-local bindings: keep the crossfade actions on their keys even
  -- though the shipped defaults now bind CUT/DIS to the ripple toggles.
  local ini = reaper.GetResourcePath() .. "/speed-reaper/bindings.ini"
  local f = io.open(ini, "w")
  check("setup: bindings.ini writable", f ~= nil, ini)
  if f then
    f:write("[bindings]\n",
            "DIS.tap = action 40916\n",  -- crossfade within time selection
            "TRANS.tap = action 41059\n", -- crossfade overlapping items
            "CUT.tap = action 41193\n",  -- remove fade in / fade out
            "TRANS_DUR.tap = action 43776\n") -- default crossfade length
    f:close()
    local reload = reaper.NamedCommandLookup("_SPEED_REAPER_RELOAD_BINDINGS")
    check("setup: reload action registered", reload > 0, "id=" .. tostring(reload))
    reaper.Main_OnCommand(reload, 0)
  end

  check("setup: tone wav written", write_tone(WAV), WAV)
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  for i = 0, 3 do reaper.InsertTrackAtIndex(i, true) end

  local function add(track, pos, len)
    local it = reaper.AddMediaItemToTrack(reaper.GetTrack(nil, track))
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos)
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", len)
    for _, k in ipairs({ "D_FADEINLEN", "D_FADEOUTLEN", "D_FADEINLEN_AUTO", "D_FADEOUTLEN_AUTO" }) do
      reaper.SetMediaItemInfo_Value(it, k, 0.0)
    end
    reaper.SetMediaItemSelected(it, false)
    return it
  end
  local c, d = add(0, 0.0, 5.0), add(0, 5.0, 5.0)
  local a, b = add(1, 0.0, 5.0), add(1, 4.0, 5.0)
  local g, h = add(2, 0.0, 5.0), add(2, 5.0, 5.0)
  local i, j = add(3, 0.0, 5.0), add(3, 4.0, 5.0)
  local function v(it, k) return reaper.GetMediaItemInfo_Value(it, k) end
  local function set(it, k, x) reaper.SetMediaItemInfo_Value(it, k, x) end
  local function select(...)
    for _, it in ipairs({ c, d, a, b, g, h, i, j }) do set(it, "B_UISEL", 0) end
    for _, it in ipairs({ ... }) do set(it, "B_UISEL", 1) end
    reaper.UpdateArrange()
  end
  reaper.SetEditCurPos(6.0, false, false)

  local trace_path = os.getenv("SPEED_REAPER_TRACE")
  local function trace_has(needle)
    local f = trace_path and io.open(trace_path, "r")
    if not f then return false end
    for line in f:lines() do
      if line:find(needle, 1, true) then f:close(); return true end
    end
    f:close()
    return false
  end

  -- DIS prep (time selection + c/d) can be set immediately: nothing
  -- disturbs it before the DIS press.
  reaper.GetSet_LoopTimeRange2(nil, true, false, 4.5, 5.5, false)
  select(c, d)

  local armed = {}
  wait_ms(3000, function()
    if not armed.trans and trace_has("cmd 40916") then
      armed.trans = true
      select(a, b)
    elseif not armed.cut and trace_has("cmd 41059") then
      armed.cut = true
      set(g, "D_FADEOUTLEN", 0.4); set(h, "D_FADEINLEN", 0.4)
      select(g, h)
    elseif not armed.dur and trace_has("cmd 41193") then
      armed.dur = true
      set(i, "D_FADEOUTLEN", 0.0); set(i, "D_FADEOUTLEN_AUTO", 1)
      set(j, "D_FADEINLEN", 0.0); set(j, "D_FADEINLEN_AUTO", 1)
      select(i, j)
    end
  end, function()
    local function near(x, y, tol) return math.abs(x - y) < (tol or 0.01) end
    check("DIS: C extended to 5.5 and D starts at 4.5 (overlap = time selection)",
          near(v(c, "D_LENGTH"), 5.5) and near(v(d, "D_POSITION"), 4.5),
          string.format("c.len=%s d.pos=%s", v(c, "D_LENGTH"), v(d, "D_POSITION")))
    check("DIS: crossfade flagged on C fade-out / D fade-in",
          v(c, "D_FADEOUTLEN_AUTO") > 0.5 and v(d, "D_FADEINLEN_AUTO") > 0.5,
          string.format("c.out_auto=%s d.in_auto=%s", v(c, "D_FADEOUTLEN_AUTO"), v(d, "D_FADEINLEN_AUTO")))
    check("TRANS: crossfade over the A/B overlap (auto fades ~1.0 s)",
          near(v(a, "D_FADEOUTLEN_AUTO"), 1.0, 0.05) and near(v(b, "D_FADEINLEN_AUTO"), 1.0, 0.05),
          string.format("a.out_auto=%s b.in_auto=%s a.len=%s b.pos=%s",
                        v(a, "D_FADEOUTLEN_AUTO"), v(b, "D_FADEINLEN_AUTO"), v(a, "D_LENGTH"), v(b, "D_POSITION")))
    check("CUT: both manual fades removed",
          v(g, "D_FADEOUTLEN") == 0 and v(h, "D_FADEINLEN") == 0,
          string.format("g.out=%s h.in=%s", v(g, "D_FADEOUTLEN"), v(h, "D_FADEINLEN")))
    local overlap = v(i, "D_POSITION") + v(i, "D_LENGTH") - v(j, "D_POSITION")
    check("TRANS_DUR: overlap shrunk to the default crossfade length (~0.02 s)",
          near(overlap, 0.02, 0.01),
          string.format("overlap=%s i.len=%s j.pos=%s", overlap, v(i, "D_LENGTH"), v(j, "D_POSITION")))
    local lines = read_trace()
    if lines then
      local want = { 40916, 41059, 41193, 43776 }
      local cmds, dump = {}, ""
      for _, l in ipairs(lines) do
        local x = l:match(" cmd (%d+)$")
        if x then table.insert(cmds, tonumber(x)); dump = dump .. x .. " " end
      end
      local ok = #cmds == #want
      for k = 1, #want do if cmds[k] ~= want[k] then ok = false end end
      check("trace: cmds are 40916, 41059, 41193, 43776", ok, "[" .. dump .. "]")
      check("trace: scenario bindings loaded n=4", find_line(lines, "bindings loaded n=4") > 0)
    end
    os.remove(WAV)
    os.remove(ini)  -- later scenarios/runs get the default file again
    finish()
  end)
end)
