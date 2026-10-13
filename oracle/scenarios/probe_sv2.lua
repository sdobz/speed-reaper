-- probe_sv2.lua — throwaway (simple_verbs finish): two unknowns.
-- (1) marker position: why did 40157 land at 0.0 when the cursor was 1.0?
--     Insert markers in three phases and record positions + play cursor:
--       M1: right after SetEditCurPos(1.0)
--       M2: after simulating the CAM2 path (item select + I_CURTAKE + undo-ish)
--       M3: after ShowConsoleMsg (AudioLevelTap's read-out dialog)
-- (2) SMTH CUT fade fields: 40916 on two [0,5] items (tone sources, two
--     tracks) with tsel [1,2]; record every fade-ish item field after.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/probe_tone_sv2.wav"
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

local function marker_positions()
  local out = {}
  local n = reaper.GetNumRegionsOrMarkers(nil)
  for i = 0, n - 1 do
    local _, _, pos = reaper.EnumProjectMarkers(i)
    table.insert(out, pos)
  end
  return table.concat(out, ",")
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
  reaper.UpdateArrange()
  record("src_file", reaper.GetMediaSourceFileName(
      reaper.GetMediaItemTake_Source(reaper.GetMediaItemTake(items[0], 0))) or "nil")

  -- Phase M1: cursor 1.0, immediate 40157.
  reaper.SetEditCurPos(1.0, false, false)
  reaper.UpdateArrange()
  record("m0_playcur", tostring(reaper.GetCursorPosition()))
  reaper.Main_OnCommand(40157, 0)
  reaper.defer(function()
    record("m1_markers", marker_positions())
    record("m1_playcur", tostring(reaper.GetCursorPosition()))

    -- Phase M2: simulate CAM2 (select A item, I_CURTAKE=1, deselect).
    reaper.SetMediaItemInfo_Value(items[0], "B_SELECTED", 1)
    reaper.SetMediaItemInfo_Value(items[0], "I_CURTAKE", 1)
    reaper.SetMediaItemInfo_Value(items[0], "B_SELECTED", 0)
    reaper.UpdateArrange()
    reaper.defer(function()
      reaper.Main_OnCommand(40157, 0)
      reaper.defer(function()
        record("m2_markers", marker_positions())
        record("m2_playcur", tostring(reaper.GetCursorPosition()))

        -- Phase M3: console dialog, then 40157.
        reaper.ShowConsoleMsg("Volume: 0.0 dB\n")
        reaper.defer(function()
          reaper.Main_OnCommand(40157, 0)
          reaper.defer(function()
            record("m3_markers", marker_positions())
            record("m3_playcur", tostring(reaper.GetCursorPosition()))

            -- Phase SMTH: tsel [1,2], 40916, dump fade fields.
            reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 2.0, false)
            local b, ts, te, ls, le =
              reaper.GetSet_LoopTimeRange2(nil, false, false, 0, 0, false)
            record("tsel", string.format("%s %s %s %s %s",
              tostring(b), tostring(ts), tostring(te),
              tostring(ls), tostring(le)))
            reaper.UpdateArrange()
            reaper.Main_OnCommand(40916, 0)
            reaper.UpdateArrange()
            reaper.defer(function()
              local fields = { "D_FADEINLEN", "D_FADEOUTLEN",
                "D_FADEINLEN_AUTO", "D_FADEOUTLEN_AUTO", "B_FADEIN_AUTO",
                "B_FADEOUT_AUTO", "D_CROSSFADELEN", "D_CROSSFADENEXT" }
              for t = 0, 1 do
                local it = items[t]
                local line = ("t" .. t):rep(1)
                for _, f in ipairs(fields) do
                  line = line .. " " .. f .. "=" ..
                    reaper.GetMediaItemInfo_Value(it, f)
                end
                record("smth_" .. t, line)
              end
              check("probe done", true, "")
              finish()
            end)
          end)
        end)
      end)
    end)
  end)
end)
