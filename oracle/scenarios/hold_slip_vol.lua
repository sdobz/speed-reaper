-- oracle/scenarios/hold_slip_vol.lua — M7 hold+rotate: SLIP SRC and
-- AUDIO LEVEL holds (vibes/33-m7-hold-rotate.md task 5b). Validates the
-- HoldStart/HoldRotate/HoldEnd path in plugin/src/verbs.cpp against
-- REAPER-visible item/take geometry: SLIP SRC shifts the take start
-- offset by one project frame (1/Fps() * playrate) without touching
-- pos/len; AUDIO LEVEL nudges the item volume +0.5 dB per step (linear
-- vol, 1.0 = 0 dB). Selection stays empty throughout.
--
-- The harness sources oracle/scenarios/hold_slip_vol.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/hold_slip_vol.txt (device
-- clock; the CLI script starts ~40 ms after plugin load, so script time
-- = device time - ~40 ms):
--   300/1200     SLIP SRC hold 0x0C (HoldStart ~650): +4 ticks at
--                700-724 -> A.startoffs 0 -> 1/30 (pos/len unchanged)
--   1600/2500    AUDIO LEVEL hold 0x2C (HoldStart ~1950): +4 ticks at
--                2000-2024 -> A.vol 1.0 -> 10^(0.5/20)
--
-- Setup: fresh instance = new project; one track with item A [0,5] on a
-- tone source (API-added items have NO take until AddTakeToMediaItem --
-- without one, the SLIP SRC nudge has nowhere to write D_STARTOFFS and
-- SetGeom's take branch is skipped; proven layout from smth_cut.lua),
-- NOTHING selected, edit cursor at 2.5 s (A is the implicit cursor
-- target, target.cpp rule 2), project 30 fps.
dofile(os.getenv("ORACLE_LIB"))

local function le16(x)
  return string.char(x % 256, math.floor(x / 256) % 256)
end
local function le32(x)
  return string.char(x % 256, math.floor(x / 256) % 256,
                     math.floor(x / 65536) % 256, math.floor(x / 16777216) % 256)
end

local res = reaper.GetResourcePath():gsub("/$", "")
local WAV = res .. "/hold_slip_vol_tone.wav"
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

  -- Pin the project frame rate to 30 fps (REAPER's default; the pcall
  -- is a no-op if the key name is unknown on this build).
  pcall(reaper.GetSet_ProjectInfo, nil, "R_FRAME_RATE", 30.0)

  local wav_ok = write_tone(WAV)
  check("setup: tone wav written", wav_ok, "")
  local src = reaper.PCM_Source_CreateFromFile(WAV)
  check("setup: tone source created", src ~= nil, "")

  reaper.InsertTrackAtIndex(0, true) -- returns bool; track is index 0
  local track = reaper.GetTrack(nil, 0)
  local a = reaper.AddMediaItemToTrack(track)
  reaper.AddTakeToMediaItem(a, true)
  reaper.SetMediaItemTake_Source(reaper.GetMediaItemTake(a, 0), src)
  reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
  reaper.SetMediaItemInfo_Value(a, "B_UISEL", 0)
  reaper.SetEditCurPos(2.5, false, false)
  reaper.UpdateArrange()
  -- API-added items have no undo point: without this, undo rolls back to
  -- an empty project.
  reaper.Undo_OnStateChange("setup")

  local function snap()
    local n = reaper.CountMediaItems(nil, track)
    local pos, len, sel, soff, vol = -1, -1, 1, -1, -1
    if n == 1 then
      local it = reaper.GetTrackMediaItem(track, 0)
      pos = reaper.GetMediaItemInfo_Value(it, "D_POSITION")
      len = reaper.GetMediaItemInfo_Value(it, "D_LENGTH")
      sel = reaper.GetMediaItemInfo_Value(it, "B_UISEL") ~= 0 and 1 or 0
      vol = reaper.GetMediaItemInfo_Value(it, "D_VOL")
      local take = reaper.GetMediaItemTake(it, 0)
      soff = take and reaper.GetMediaItemTakeInfo_Value(take, "D_STARTOFFS")
                   or -1
    end
    return n, pos, len, sel, soff, vol
  end

  local n0, p0, l0 = snap()
  check("setup: one item A at [0,5]", n0 == 1 and p0 == 0.0 and l0 == 5.0,
        string.format("n=%d pos=%s len=%s", n0, tostring(p0), tostring(l0)))
  check("setup: edit cursor at 2.5",
        math.abs(reaper.GetCursorPosition() - 2.5) < 1e-6,
        "GetCursorPosition=" .. tostring(reaper.GetCursorPosition()))

  -- Sample every defer cycle until the second hold has landed (last
  -- device event ~2500 ms; script clock is ~40 ms behind). The
  -- selection must stay empty through every sample.
  local bad_count, bad_sel, t0 = 0, 0, reaper.time_precise()
  local last
  wait_ms(3200, function()
    local n, pos, len, sel = snap()
    last = { t = (reaper.time_precise() - t0) * 1000, n = n,
             pos = pos, len = len }
    if n ~= 1 then bad_count = bad_count + 1 end
    if sel ~= 0 then bad_sel = bad_sel + 1 end
  end, function()
    local n, pos, len, sel, soff, vol = snap()
    local frame = 1.0 / 30.0
    local nsoff = 1.0 * frame            -- 1 step, playrate 1.0
    local nvol = 10 ^ (0.5 / 20.0)       -- +0.5 dB over 1.0

    check("item count stays 1 throughout (no splits, no duplicates)",
          bad_count == 0,
          "samples_with_other_count=" .. bad_count)
    check("selection stays empty throughout both holds",
          bad_sel == 0 and sel == 0,
          "samples_with_selection=" .. bad_sel .. " final_sel=" .. sel)
    check("SLIP SRC hold +4 ticks: A.startoffs ~= +1/30",
          n == 1 and math.abs(soff - nsoff) < 0.001,
          string.format("startoffs=%s want=%s (last_sample %s)",
                        tostring(soff), tostring(nsoff),
                        last and string.format(
                          "t=%.0f pos=%s len=%s", last.t,
                          tostring(last.pos), tostring(last.len))
                          or "none"))
    check("SLIP SRC hold: A.pos/len unchanged at [0,5]",
          n == 1 and math.abs(pos - 0.0) < 0.001
                  and math.abs(len - 5.0) < 0.001,
          string.format("pos=%s len=%s", tostring(pos), tostring(len)))
    check("AUDIO LEVEL hold +4 ticks: A.vol ~= 10^(0.5/20)",
          n == 1 and math.abs(vol - nvol) < 1e-4,
          string.format("vol=%s want=%s", tostring(vol), tostring(nvol)))

    -- Trace: SLIP SRC hold (000c) then AUDIO LEVEL hold (002c); each
    -- hold traces hold_start, >=1 hold_rotate (one per wheel event) and
    -- hold_end, in that order. Collapse consecutive repeats.
    local lines = read_trace()
    if lines then
      local kinds = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) (hold_[a-z]+)$")
        if k then table.insert(kinds, k .. " " .. kind) end
      end
      local collapsed = {}
      for _, kk in ipairs(kinds) do
        if collapsed[#collapsed] ~= kk then
          table.insert(collapsed, kk)
        end
      end
      local want = { "000c hold_start", "000c hold_rotate", "000c hold_end",
                     "002c hold_start", "002c hold_rotate",
                     "002c hold_end" }
      local ok = #collapsed == #want
      for i = 1, #want do
        if collapsed[i] ~= want[i] then ok = false end
      end
      check("trace: SLIP SRC hold then AUDIO LEVEL hold (start/rotate/end each)",
            ok, "got [" .. table.concat(collapsed, " | ") .. "]")

      -- No command ran: hold verbs act via the project API, not RunCommand.
      local cmds = 0
      for _, l in ipairs(lines) do
        if l:match(" cmd %d+$") then cmds = cmds + 1 end
      end
      check("trace: no cmd lines (hold verbs use the project API)",
            cmds == 0, "cmd_lines=" .. cmds)
    end

    finish()
  end)
end)
