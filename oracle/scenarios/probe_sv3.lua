-- probe_sv3.lua — throwaway: does 40916 crossfade when BOTH items are
-- selected? (KVR 2018: "Both items selected (and within time selection)
-- you want to crossfade? Works here".) Same layout as probe_sv2.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_sv3.wav"
local function write_tone(path)
  local f = io.open(path, "wb")
  if not f then return false end
  local sr, secs = 44100, 6
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

start("settled", function()
  record("wav_written", tostring(write_tone(WAV)))
  local n = reaper.CountTracks(nil)
  for _ = 1, 2 - n do reaper.InsertTrackAtIndex(n, true) end
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  local items = {}
  for t = 0, 1 do
    local tr = reaper.GetTrack(nil, t)
    local it = reaper.AddMediaItemToTrack(tr)
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", 0.0)
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 5.0)
    items[t] = it
  end
  reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 2.0, false)
  reaper.UpdateArrange()

  local function dump(tag)
    local fields = { "D_FADEINLEN", "D_FADEOUTLEN",
      "D_FADEINLEN_AUTO", "D_FADEOUTLEN_AUTO", "B_FADEIN_AUTO",
      "B_FADEOUT_AUTO", "D_CROSSFADELEN", "D_CROSSFADENEXT" }
    for t = 0, 1 do
      local key = tag .. "_t" .. t
      local line = key
      for _, f in ipairs(fields) do
        line = line .. " " .. f .. "=" ..
          reaper.GetMediaItemInfo_Value(items[t], f)
      end
      record(key, line)
    end
  end

  dump("before")
  -- SELECT both items: this Lua build lacks SelectMediaItemInProject and
  -- SetMediaItemInfo_Value "B_SELECTED" writes are a no-op — use 40717
  -- "Item: Select all items in current time selection" (both items sit in
  -- tsel [1,2]).
  reaper.Main_OnCommand(40717, 0)
  reaper.UpdateArrange()
  reaper.defer(function()
    record("sel_count", tostring(reaper.CountSelectedMediaItems(nil)))
    reaper.Main_OnCommand(40916, 0)
    reaper.UpdateArrange()
    reaper.defer(function()
      dump("after_40916")
      check("probe done", true, "")
      finish()
    end)
  end)
end)
