-- probe_xf2.lua — throwaway probe, v8: bisect which command hangs.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

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

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_xf2.wav"
record("wav_written", tostring(write_tone(WAV)))

local function mkitem(tr, pos, len)
  local it = reaper.AddMediaItemToTrack(tr)
  reaper.AddTakeToMediaItem(it, true)
  reaper.SetMediaItemTake_Source(
    reaper.GetMediaItemTake(it, 0),
    reaper.PCM_Source_CreateFromFile(WAV))
  reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos)
  reaper.SetMediaItemInfo_Value(it, "D_LENGTH", len)
  return it
end

local function itemline(it)
  local p = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
  local l = reaper.GetMediaItemInfo_Value(it, "D_LENGTH")
  local fain = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN")
  local fain_a = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN_AUTO")
  local fout = reaper.GetMediaItemInfo_Value(it, "D_FADEOUTLEN")
  local fout_a = reaper.GetMediaItemInfo_Value(it, "D_FADEOUTLEN_AUTO")
  return string.format("[%g,%g] fin=%g finA=%g fout=%g foutA=%g",
                       p, p + l, fain, fain_a, fout, fout_a)
end

start("settled", function()
  local l2 = reaper.GetSet_LoopTimeRange2
  local n = reaper.CountTracks(nil)
  for _ = 1, 4 - n do reaper.InsertTrackAtIndex(n, true) end
  local tr = {}
  for i = 0, 3 do tr[i] = reaper.GetTrack(nil, i) end

  -- T1: control action 40026 "Item: Trim items to time selection".
  local s = mkitem(tr[0], 0.0, 5.0)
  l2(nil, true, false, 1.0, 2.0, false)
  reaper.UpdateArrange()
  reaper.Main_OnCommand(40026, 0)
  reaper.UpdateArrange()
  record("T1_40026_trim", itemline(s))

  -- T2: 40916 on overlapping sourced items, NO state chunk calls.
  local a = mkitem(tr[1], 0.0, 5.0)
  local c = mkitem(tr[2], 0.0, 5.0)
  l2(nil, true, false, 1.0, 2.0, false)
  reaper.UpdateArrange()
  record("T2_before_A", itemline(a))
  reaper.Main_OnCommand(40916, 0)
  reaper.UpdateArrange()
  record("T2_after_A", itemline(a))
  record("T2_after_C", itemline(c))

  -- T3: the 40041 toggle (suspected dialog hang).
  reaper.Main_OnCommand(40041, 0)
  reaper.UpdateArrange()
  record("T3_40041_done", "reached")

  check("probe done", true, "see info records")
  finish()
end)
