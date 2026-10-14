-- oracle/scenarios/leds_cam.lua — M8 LED bar (vibes/34-m8-led-bar.md
-- task 4b): the CAM take bit of the main-panel mask, end to end. Lua
-- drives REAPER state (creates 3-take items, sets I_CURTAKE, selects),
-- the plugin's ~30 Hz timer poll re-reads the shared active take of the
-- selected items and traces `led main=<hex>` on change; the scenario
-- asserts the traced value after each step, within 1.5 s.
--
-- The harness sources oracle/scenarios/leds_cam.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/leds_cam.txt (device clock;
-- the script starts ~40 ms after plugin load). The device only taps ESC
-- (0x31, single tap -> cmd 40635, time selection removed — no main-panel
-- LED effect) for device-path coverage; the CAM1..CAM9 buttons are
-- deliberately NOT used (CamSetTake would change active takes and race
-- the Lua steps). Steps are >= 1.5 s apart:
--   ~0     fresh profile (snap ON, nothing selected)
--          -> forced first `led main=` read at load: snap bit only,
--             no CAM bit
--   1600   3-take item, take 2 active (I_CURTAKE=1), selected
--          -> CAM2 (bit 15, 0x8000)
--   3200   second 3-take item, take 1 active (I_CURTAKE=0), also
--          selected -> mixed takes -> no CAM bit
--
-- Trace lines are "<ms since plugin load> <msg>" (plugin/src/trace.cpp);
-- reaper.time_precise() is a different epoch, so the offset is calibrated
-- on the `log connected` line at body start (both clocks are monotonic and
-- run at the same rate).
dofile(os.getenv("ORACLE_LIB"))

local function trace_lines()
  local path = os.getenv("SPEED_REAPER_TRACE")
  local f = path and io.open(path, "r")
  if not f then return nil end
  local lines = {}
  for line in f:lines() do lines[#lines + 1] = line end
  f:close()
  return lines
end

-- ms offset: script_time(line) = line_ms + off
local function calibrate()
  local lines = trace_lines()
  if not lines then return nil end
  local last_ms = nil
  for _, l in ipairs(lines) do
    local ms = l:match("^(%d+) log connected")
    if ms then last_ms = tonumber(ms) end
  end
  if last_ms == nil then return nil end
  return reaper.time_precise() * 1000 - last_ms
end

-- 3-take item at `pos` on `track`, take index `curtake` (0-based) active,
-- selected. Empty takes are fine: ReadLedState only reads I_CURTAKE and
-- IsMediaItemSelected.
local function make_item(track, pos, curtake)
  local it = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(it, "D_POSITION", pos)
  reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 1.0)
  reaper.AddTakeToMediaItem(it, true)  -- items start with 0 takes
  reaper.AddTakeToMediaItem(it, true)  -- 3 takes total; I_CURTAKE set below
  reaper.AddTakeToMediaItem(it, true)
  reaper.SetMediaItemInfo_Value(it, "I_CURTAKE", curtake)
  reaper.SetMediaItemSelected(it, true)
  reaper.UpdateItemInProject(it)
  reaper.UpdateArrange()
  return it
end

start("device", function()
  record("app_version", reaper.GetAppVersion())
  local off = calibrate()
  check("calibrated trace offset (log connected found)", off ~= nil,
        "off=" .. tostring(off))
  if off == nil then finish() return end

  -- Fresh-profile defaults: snap ON (0x20), nothing selected (no CAM bit).
  local snap = reaper.GetToggleCommandState(1157)
  local snap_bit = snap == 1 and 0x20 or 0x00
  local cam2 = 0x8000                    -- LED_CAM2 = bit 15
  local v0 = snap_bit                    -- nothing selected: no CAM bit
  local v1 = snap_bit | cam2             -- take 2 shared -> CAM2
  local v2 = snap_bit                    -- mixed takes: no CAM bit

  local track, it1, it2 = nil, nil, nil
  local reads = {}
  local ts1, ts2 = nil, nil
  local done = {}
  local t_start = reaper.time_precise()

  wait_ms(5200, function()
    local st = (reaper.time_precise() - t_start) * 1000

    -- v0 (before any change): every `led main=` line so far must be the
    -- fresh-profile mask — nothing selected, so no CAM bit anywhere.
    if not done.v0 and st >= 1000 then
      done.v0 = true
      local lines = trace_lines() or {}
      local n, bad = 0, nil
      for _, l in ipairs(lines) do
        local ms, hx = l:match("^(%d+) led main=([0-9a-f]+)$")
        if ms then
          n = n + 1
          local v = tonumber(hx, 16)
          if bad == nil and v ~= v0 then bad = v end
        end
      end
      check("nothing selected: `led main=` traced, no CAM bit",
            n >= 1 and bad == nil,
            string.format("lines=%d want=%d first_bad=%s snap=%s",
                          n, v0, tostring(bad), tostring(snap)))
    end

    if not done.s1 and st >= 1600 then
      done.s1 = true; ts1 = st + off
      reaper.InsertTrackAtIndex(0, false) -- returns bool; track is index 0
      track = reaper.GetTrack(nil, 0)
      it1 = make_item(track, 0.0, 1)       -- 3 takes, take 2 active
      reads.takes1 = reaper.CountTakes(it1)
      reads.cur1 = reaper.GetMediaItemInfo_Value(it1, "I_CURTAKE")
      reads.sel1 = reaper.IsMediaItemSelected(it1)
    elseif not done.s2 and st >= 3200 then
      done.s2 = true; ts2 = st + off
      it2 = make_item(track, 2.0, 0)       -- 3 takes, take 1 active
      reads.takes2 = reaper.CountTakes(it2)
      reads.cur2 = reaper.GetMediaItemInfo_Value(it2, "I_CURTAKE")
      reads.sel2 = reaper.IsMediaItemSelected(it2)
      reads.sel1_after = reaper.IsMediaItemSelected(it1)
    end
  end, function()
    local lines = read_trace()
    if lines then
      -- The device tap: ESC single tap -> gesture + cmd 40635.
      local gest, cmd = 0, 0
      for _, l in ipairs(lines) do
        if l:find(" gesture 0031 tap", 1, true) then gest = 1 end
        if l:find(" cmd 40635", 1, true) then cmd = 1 end
      end
      check("device: ESC tap traced (gesture 0031 tap -> cmd 40635)",
            gest == 1 and cmd == 1,
            string.format("gest=%d cmd=%d", gest, cmd))

      check("item 1: 3 takes, take 2 active (I_CURTAKE=1), selected",
            reads.takes1 == 3 and reads.cur1 == 1 and reads.sel1 == true,
            string.format("takes=%s cur=%s sel=%s",
                          tostring(reads.takes1), tostring(reads.cur1),
                          tostring(reads.sel1)))
      check("item 2: 3 takes, take 1 active (I_CURTAKE=0), both selected",
            reads.takes2 == 3 and reads.cur2 == 0 and reads.sel2 == true
              and reads.sel1_after == true,
            string.format("takes=%s cur=%s sel=%s sel1=%s",
                          tostring(reads.takes2), tostring(reads.cur2),
                          tostring(reads.sel2), tostring(reads.sel1_after)))

      -- All `led main=` lines with script time (offset-calibrated).
      local led = {}
      for _, l in ipairs(lines) do
        local ms, b = l:match("^(%d+) led main=([0-9a-f]+)$")
        if ms then table.insert(led, { t = tonumber(ms) + off, v = tonumber(b, 16) }) end
      end

      local function first_val(v, from)
        for i = from or 1, #led do
          if led[i].v == v then return i end
        end
        return 0
      end
      local i1 = first_val(v1)
      local i2 = first_val(v2, i1 + 1)

      local function within(i, ts, label)
        check(label .. ": `led main=` appears within 1.5 s of the step",
              i ~= 0 and ts ~= nil and led[i].t >= ts - 300 and led[i].t <= ts + 1500,
              string.format("i=%d t=%s step_t=%s", i,
                            i ~= 0 and string.format("%.0f", led[i].t) or "nil",
                            tostring(ts)))
      end
      within(i1, ts1, "3-take item take 2 selected: CAM2 (0x" ..
              string.format("%x", v1) .. ")")
      within(i2, ts2, "two selected items, different takes: no CAM bit (0x" ..
              string.format("%x", v2) .. ")")

      check("order: baseline line < CAM2 line < no-CAM line",
            i1 > 1 and i2 > i1,
            string.format("i1=%d i2=%d n=%d", i1, i2, #led))
    end

    finish()
  end)
end)
