-- probe_sv6.lua — throwaway: 40916 boundary-in-tsel variants, 40717 select.
-- V1: [0,5]x2 (KNOWN no-op control). V2: [1.2,1.8]+[1.8,2.4] (both inside tsel).
-- V3: [1.2,5]+[1.8,6] (starts inside, ends outside). V4: [0,1.8]+[1.2,5].
dofile(os.getenv("ORACLE_LIB"))

local function le16(x) return string.char(x % 256, math.floor(x / 256) % 256) end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end
local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_sv6.wav"
local f = io.open(WAV, "wb")
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

local FIELDS = { "D_FADEINLEN_AUTO", "D_FADEOUTLEN_AUTO", "D_CROSSFADELEN",
  "D_CROSSFADENEXT" }
local function itemline(it)
  local line = "it"
  for _, k in ipairs(FIELDS) do
    line = line .. " " .. k .. "=" .. reaper.GetMediaItemInfo_Value(it, k)
  end
  return line
end

start("settled", function()
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  local n = reaper.CountTracks(nil)
  for _ = 1, 2 - n do reaper.InsertTrackAtIndex(n, true) end
  reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 2.0, false)
  reaper.UpdateArrange()

  local function phase(tag, p1, l1, p2, l2, nextfn)
    reaper.defer(function()
      reaper.Main_OnCommand(40769, 0)
      reaper.Main_OnCommand(40717, 0)
      reaper.Main_OnCommand(40002, 0) -- remove selected
      reaper.UpdateArrange()
      reaper.defer(function()
        local its = {}
        for t, p, l in ipairs({ { 0, p1, l1 }, { 1, p2, l2 } }) do
          local it = reaper.AddMediaItemToTrack(reaper.GetTrack(nil, t))
          reaper.AddTakeToMediaItem(it, true)
          reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
          reaper.SetMediaItemInfo_Value(it, "D_POSITION", p)
          reaper.SetMediaItemInfo_Value(it, "D_LENGTH", l)
          its[#its + 1] = it
        end
        reaper.UpdateArrange()
        reaper.defer(function()
          reaper.Main_OnCommand(40717, 0)
          reaper.UpdateArrange()
          reaper.defer(function()
            reaper.Main_OnCommand(40916, 0)
            reaper.UpdateArrange()
            reaper.defer(function()
              record(tag .. "_i1", itemline(its[1]))
              record(tag .. "_i2", itemline(its[2]))
              nextfn()
            end)
          end)
        end)
      end)
    end)
  end

  phase("V1_ctrl", 0, 5, 0, 5, function()
    phase("V2_in", 1.2, 0.6, 1.8, 0.6, function()
      phase("V3_right", 1.2, 3.8, 1.8, 4.2, function()
        phase("V4_left", 0, 1.8, 1.2, 3.8, function()
          check("probe done", true, "")
          finish()
        end)
      end)
    end)
  end)
end)
