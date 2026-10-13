-- probe_xf3.lua — throwaway: bisect which call hangs in headless REAPER.
-- PROBE_STEP env: 1 = 40026 trim; 2 = GetItemStateChunk; 3 = 40041 toggle.
dofile(os.getenv("ORACLE_LIB"))

local step = tonumber(os.getenv("PROBE_STEP")) or 0

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_xf3.wav"
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

record("wav_written", tostring(write_tone(WAV)))

start("settled", function()
  record("step", tostring(step))
  local n = reaper.CountTracks(nil)
  for _ = 1, 2 - n do reaper.InsertTrackAtIndex(n, true) end
  local tr = reaper.GetTrack(nil, 0)
  local it = reaper.AddMediaItemToTrack(tr)
  reaper.AddTakeToMediaItem(it, true)
  reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0),
                                 reaper.PCM_Source_CreateFromFile(WAV))
  reaper.SetMediaItemInfo_Value(it, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 5.0)
  reaper.UpdateArrange()

  if step == 1 then
    reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 2.0, false)
    reaper.UpdateArrange()
    record("pre_40026", "reached")
    reaper.Main_OnCommand(40026, 0)
    reaper.UpdateArrange()
    record("post_40026", "reached")
  elseif step == 2 then
    record("pre_chunk", "reached")
    local ok, c = reaper.GetItemStateChunk(it, "", 8192, false)
    record("chunk_ok", tostring(ok))
    record("chunk_len", c and tostring(#c) or "nil")
    record("post_chunk", "reached")
  elseif step == 3 then
    record("pre_40041", "reached")
    reaper.Main_OnCommand(40041, 0)
    reaper.UpdateArrange()
    record("post_40041", "reached")
  end

  check("probe done", true, "step " .. step)
  finish()
end)
