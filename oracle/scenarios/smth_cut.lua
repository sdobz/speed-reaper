-- oracle/scenarios/smth_cut.lua — M6 SMTH CUT (vibes/32-m6-simple-verbs.md
-- task 6b). One gesture: SMTH CUT 0x11 tap (~410 ms) -> cmd 40916 "Item:
-- Crossfade items within time selection" (verbs.cpp SmthCutTap; the verb
-- no-ops without a time selection).
--
-- The harness sources oracle/scenarios/smth_cut.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/smth_cut.txt (device clock;
-- the CLI script starts ~40 ms after plugin load, so script time = device
-- time - ~40 ms):
--   ~410         SMTH CUT 0x11 tap         -> cmd 40916, auto-fades at the
--                                             1.5 s boundary (inside tsel)
--
-- Setup directly through the API (no PLACE ON TOP): one track, item A [0,1.5]
-- and item C [1.5,5] meeting at 1.5, tone PCM sources on both, both items
-- selected via 40717, time selection [1,2]. This is the proven layout
-- (probe_sv4.lua): 40916 creates auto-fades ONLY at item boundaries strictly
-- inside the time selection, and only with real audio sources (empty takes
-- no-op), giving item A D_FADEOUTLEN_AUTO ~= 0.99 and item C
-- D_FADEINLEN_AUTO ~= 0.99. See docs/notes/m5-observations.md ## M6.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/smth_cut_tone.wav"
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

start("device", function()
  record("app_version", reaper.GetAppVersion())
  local wav_ok = write_tone(WAV)
  record("wav_written", wav_ok)
  check("setup: tone wav written", wav_ok, "")
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  check("setup: tone source created", src ~= nil, "")

  -- One track, items A [0,1.5] and C [1.5,5] meeting at 1.5, tone on both.
  local n = reaper.CountTracks(nil)
  for _ = 1, 1 - n do reaper.InsertTrackAtIndex(0, true) end
  local tr = reaper.GetTrack(nil, 0)
  local function build_item(pos, len)
    local it = reaper.AddMediaItemToTrack(tr)
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(it, 0), src)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos)
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", len)
    return it
  end
  local itemA = build_item(0.0, 1.5)
  local itemC = build_item(1.5, 3.5)
  reaper.UpdateArrange()
  -- Time selection [1,2]: the 1.5 s meeting boundary lies strictly inside.
  reaper.GetSet_LoopTimeRange2(nil, true, false, 1.0, 2.0, false)
  reaper.UpdateArrange()
  check("setup: two items on one track",
        reaper.CountMediaItems(nil, tr) == 2,
        "items=" .. reaper.CountMediaItems(nil, tr))

  -- Select both items via 40717 (as in probe_sv4); the tap lands ~410 ms
  -- device time, so this must settle well before then.
  reaper.defer(function()
    reaper.Main_OnCommand(40717, 0) -- select items in time selection
    reaper.UpdateArrange()
    reaper.defer(function()
      local sel = reaper.CountSelectedMediaItems(nil)
      record("selected_items", sel)
      check("setup: both items selected (40717)", sel == 2, "sel=" .. sel)

      -- Wait past the gesture (tap ~370 ms script time).
      wait_ms(1500, nil, function()
        local finA = reaper.GetMediaItemInfo_Value(itemA, "D_FADEINLEN_AUTO")
        local foutA = reaper.GetMediaItemInfo_Value(itemA, "D_FADEOUTLEN_AUTO")
        local finC = reaper.GetMediaItemInfo_Value(itemC, "D_FADEINLEN_AUTO")
        local foutC = reaper.GetMediaItemInfo_Value(itemC, "D_FADEOUTLEN_AUTO")
        record("A_D_FADEINLEN_AUTO", finA)
        record("A_D_FADEOUTLEN_AUTO", foutA)
        record("C_D_FADEINLEN_AUTO", finC)
        record("C_D_FADEOUTLEN_AUTO", foutC)
        check("SMTH CUT: some item's D_FADEINLEN_AUTO/D_FADEOUTLEN_AUTO > 0",
              finA > 0 or foutA > 0 or finC > 0 or foutC > 0,
              string.format("A in=%g out=%g C in=%g out=%g",
                            finA, foutA, finC, foutC))

        -- Trace checks (build/oracle/trace_smth_cut.txt, "<ms> <msg>").
        local lines = read_trace()
        if lines then
          local got_cmd, got_gest = false, false
          for _, l in ipairs(lines) do
            if l:find("cmd 40916", 1, true) then got_cmd = true end
            if l:match(" gesture 0011 tap$") then got_gest = true end
          end
          check("trace: gesture 0011 tap (SMTH CUT)", got_gest, "")
          check("trace: cmd 40916 (crossfade within time selection)",
                got_cmd, "")
        end
        finish()
      end)
    end)
  end)
end)
