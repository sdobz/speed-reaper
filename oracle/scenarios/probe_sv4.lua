-- probe_sv4.lua — throwaway: 40916/41059 config matrix with tone sources
-- and 40717 selection. Phases run sequentially; items removed between
-- phases with 40002 (remove selected). Every phase dumps the fade fields
-- of its items after the command.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_sv4.wav"
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

local FIELDS = { "D_FADEINLEN", "D_FADEOUTLEN", "D_FADEINLEN_AUTO",
  "D_FADEOUTLEN_AUTO", "B_FADEIN_AUTO", "B_FADEOUT_AUTO",
  "D_CROSSFADELEN", "D_CROSSFADENEXT", "D_CROSSFADESHAPE" }

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

  local function build(specs)
    -- specs: { {tr, pos, len}, ... }; returns item list
    local its = {}
    for i, sp in ipairs(specs) do
      local tr = reaper.GetTrack(nil, sp[1])
      local it = reaper.AddMediaItemToTrack(tr)
      reaper.AddTakeToMediaItem(it, true)
      reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
      reaper.SetMediaItemInfo_Value(it, "D_POSITION", sp[2])
      reaper.SetMediaItemInfo_Value(it, "D_LENGTH", sp[3])
      its[i] = it
    end
    reaper.UpdateArrange()
    return its
  end

  local function clear_items()
    reaper.Main_OnCommand(40769, 0) -- unselect all
    reaper.UpdateArrange()
    reaper.Main_OnCommand(40717, 0)
    reaper.UpdateArrange()
    reaper.Main_OnCommand(40002, 0) -- remove selected items
    reaper.UpdateArrange()
  end

  local function phase(tag, specs, tsel, cmd, nextfn)
    clear_items()
    reaper.defer(function()
      local its = build(specs)
      reaper.GetSet_LoopTimeRange2(nil, true, false, tsel[1], tsel[2], false)
      reaper.UpdateArrange()
      reaper.defer(function()
        reaper.Main_OnCommand(40717, 0) -- select items in tsel
        reaper.UpdateArrange()
        reaper.defer(function()
          record(tag .. "_sel", tostring(reaper.CountSelectedMediaItems(nil)))
          reaper.Main_OnCommand(cmd, 0)
          reaper.UpdateArrange()
          reaper.defer(function()
            for i, it in ipairs(its) do
              record(string.format("%s_item%d", tag, i), itemline(it))
            end
            nextfn()
          end)
        end)
      end)
    end)
  end

  -- L2: simple_verbs layout [0,5]x2 two tracks, tsel [1,2], 41059
  phase("L2_41059", { { 0, 0, 5 }, { 1, 0, 5 } }, { 1, 2 }, 41059, function()
    -- L3: items fully inside tsel [1,2], 40916
    phase("L3_inside", { { 0, 1.0, 0.5 }, { 1, 1.5, 0.5 } }, { 1, 2 }, 40916, function()
      -- L4: meeting at 2.5, tsel [1,4], 40916
      phase("L4_meet", { { 0, 0, 2.5 }, { 1, 2.5, 2.5 } }, { 1, 4 }, 40916, function()
        -- L5: overlap on ONE track (lanes), tsel [1,4], 40916
        phase("L5_lane", { { 0, 0, 3 }, { 0, 2, 3 } }, { 1, 4 }, 40916, function()
          clear_items()
          reaper.defer(function()
            check("probe done", true, "")
            finish()
          end)
        end)
      end)
    end)
  end)
end)
