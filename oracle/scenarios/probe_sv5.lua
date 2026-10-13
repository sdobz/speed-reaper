-- probe_sv5.lua — throwaway: EXACT simple_verbs post-PLACE-ON-TOP layout:
-- track 0 = C (copy, unnamed), track 1 = "A", track 2 = "B". C moved to
-- [1.5,6.5] so its start edge sits inside tsel [1,2]. Selection via track
-- select (C+A) + 40718 "Select all items on selected tracks in current
-- time selection" (exactly A+C items, B excluded). Then 40916.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_sv5.wav"
local function write_tone(path)
  local f = io.open(path, "wb")
  if not f then return false end
  local sr, secs = 44100, 8
  local ns = sr * secs
  local hdr = "RIFF" .. le32(36 + ns * 2) .. "WAVEfmt " .. le32(16)
              .. le16(1) .. le16(sr) .. le32(sr * 2) .. le16(2) .. le16(16)
              .. "data" .. le32(ns * 2)
  f:write(hdr)
  for i = 0, ns - 1 do
    local s = math.sin(2 * math.pi * 440 * i / sr) * 16000
    f:write(le16(math.floor(s) + 32768))
  end
  f:close()
  return true
end

local FIELDS = { "D_FADEINLEN", "D_FADEOUTLEN", "D_FADEINLEN_AUTO",
  "D_FADEOUTLEN_AUTO", "B_FADEIN_AUTO", "B_FADEOUT_AUTO",
  "D_CROSSFADELEN", "D_CROSSFADENEXT" }

local function itemline(it)
  local line = "it"
  for _, f in ipairs(FIELDS) do
    line = line .. " " .. f .. "=" .. reaper.GetMediaItemInfo_Value(it, f)
  end
  return line
end

start("settled", function()
  record("wav_written", tostring(write_tone(WAV)))
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  local n = reaper.CountTracks(nil)
  for _ = 1, 3 - n do reaper.InsertTrackAtIndex(n, true) end
  local items = {}
  local pos = { 1.5, 0.0, 0.0 }  -- C, A, B
  for t = 0, 2 do
    local tr = reaper.GetTrack(nil, t)
    local it = reaper.AddMediaItemToTrack(tr)
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos[t + 1])
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 5.0)
    items[t] = it
  end
  reaper.UpdateArrange()

  reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 2.0, false)
  reaper.UpdateArrange()
  reaper.defer(function()
    reaper.Main_OnCommand(40297, 0) -- unselect all tracks
    reaper.Main_OnCommand(40717, 0) -- select ALL items in tsel (A, B, C)
    reaper.UpdateArrange()
    reaper.defer(function()
      -- which items got selected?
      local sel = {}
      for t = 0, 2 do
        table.insert(sel, reaper.GetMediaItemInfo_Value(items[t], "B_SELECTED"))
      end
      record("sel_C_A_B", table.concat(sel, ","))
      record("sel_count", tostring(reaper.CountSelectedMediaItems(nil)))
      reaper.Main_OnCommand(40916, 0)
      reaper.UpdateArrange()
      reaper.defer(function()
        record("C", itemline(items[0]))
        record("A", itemline(items[1]))
        record("B", itemline(items[2]))
        check("probe done", true, "")
        finish()
      end)
    end)
  end)
end)
