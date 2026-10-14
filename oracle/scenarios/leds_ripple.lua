-- oracle/scenarios/leds_ripple.lua — M8 LED bar (vibes/34-m8-led-bar.md
-- task 4a): the ripple and trim-behind bits of the main-panel mask, end to
-- end. Lua drives REAPER state (Main_OnCommand), the plugin's ~30 Hz timer
-- poll re-reads it and traces `led main=<hex>` on change; the scenario
-- asserts the traced value after each step, within 1.5 s.
--
-- The harness sources oracle/scenarios/leds_ripple.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/leds_ripple.txt (device clock;
-- the script starts ~40 ms after plugin load). The device only taps ESC
-- (0x31, single tap -> cmd 40635, time selection removed — no main-panel
-- LED effect) for device-path coverage; there are no ripple/trim-behind
-- buttons, so all state changes are Lua-driven, >= 1.5 s apart:
--   ~0     fresh profile (snap ON, ripple off, trim-behind off)
--          -> forced first `led main=` read at load: snap bit only
--   1600   Main_OnCommand(40310)  Set ripple per-track  -> CUT (bit 1, 0x02)
--   3200   Main_OnCommand(40311)  Set ripple all tracks -> DIS (bit 2, 0x04)
--   4800   Main_OnCommand(41117)  toggle trim-behind on -> LIVE_OWR (bit 9, 0x200)
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

start("device", function()
  record("app_version", reaper.GetAppVersion())
  local off = calibrate()
  check("calibrated trace offset (log connected found)", off ~= nil,
        "off=" .. tostring(off))
  if off == nil then finish() return end

  -- Fresh-profile defaults: snap ON (0x20), ripple off, trim-behind off.
  local snap = reaper.GetToggleCommandState(1157)
  local snap_bit = snap == 1 and 0x20 or 0x00
  local v0 = snap_bit                       -- off: no CUT/DIS bits
  local v1 = snap_bit | 0x02                -- per-track: CUT
  local v2 = snap_bit | 0x04                -- all: DIS
  local v3 = snap_bit | 0x04 | 0x200        -- + trim-behind: LIVE_OWR

  -- absolute script ms of each step (same epoch as the calibrated trace
  -- times: relative st plus the calibrated off)
  local ts1, ts2, ts3 = nil, nil, nil
  local tb_after = nil
  local done = {}
  local t_start = reaper.time_precise()

  wait_ms(6200, function()
    local st = (reaper.time_precise() - t_start) * 1000

    -- v0 (before any change): every `led main=` line so far must be the
    -- fresh-profile mask — ripple off, so no CUT/DIS bits anywhere.
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
      check("ripple off: `led main=` traced, no CUT/DIS bits",
            n >= 1 and bad == nil,
            string.format("lines=%d want=%d first_bad=%s snap=%s",
                          n, v0, tostring(bad), tostring(snap)))
    end

    if not done.s1 and st >= 1600 then
      done.s1 = true; ts1 = st + off
      reaper.Main_OnCommand(40310, 0)  -- Set ripple editing per-track
    elseif not done.s2 and st >= 3200 then
      done.s2 = true; ts2 = st + off
      reaper.Main_OnCommand(40311, 0)  -- Set ripple editing all tracks
    elseif not done.s3 and st >= 4800 then
      done.s3 = true; ts3 = st + off
      reaper.Main_OnCommand(41117, 0)  -- Options: Trim content behind ...
      tb_after = reaper.GetToggleCommandState(41117)
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
      local i3 = first_val(v3, i2 + 1)

      local function within(i, ts, label, want)
        check(label .. ": `led main=` appears within 1.5 s of the step",
              i ~= 0 and ts ~= nil and led[i].t >= ts - 300 and led[i].t <= ts + 1500,
              string.format("i=%d t=%s step_t=%s want=%d", i,
                            i ~= 0 and string.format("%.0f", led[i].t) or "nil",
                            tostring(ts), want))
      end
      within(i1, ts1, "40310 per-track: CUT", v1)
      within(i2, ts2, "40311 all: DIS", v2)
      within(i3, ts3, "41117 trim-behind on: LIVE_OWR", v3)

      check("trim-behind state read: 41117 == 1 after the toggle",
            tb_after == 1, "tb_after=" .. tostring(tb_after))

      check("order: off line < CUT line < DIS line < LIVE_OWR line",
            i1 > 1 and i2 > i1 and i3 > i2,
            string.format("i1=%d i2=%d i3=%d n=%d", i1, i2, i3, #led))
    end

    finish()
  end)
end)
