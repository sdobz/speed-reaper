-- oracle/scenarios/leds_binding_driven.lua — the LED follows the *binding*
-- (vibes/43-led-refactor.md task 9, design § Mask: BindingLedMask). The
-- default bindings leave CLOSE_UP unbound (commented out in
-- default_bindings.ini), so CLOSE_UP's bit (LED_CLOSE_UP = bit 0, 0x01) is
-- dark by default even with ripple-all on. This scenario writes a
-- bindings.ini rebinding CLOSE_UP.tap to `action 40311` (set ripple
-- editing all tracks — its registry state read: RippleMode() == 2), runs
-- "Speed Reaper: reload bindings", then drives the ripple mode with Lua
-- (Main_OnCommand) and asserts the traced `led main=<hex>` lines:
--   CLOSE_UP's bit (0x01) lights iff ripple-all is on — it tracks the
--     state of the *bound* verb, not the key's printed label;
--   SNAP's own bit (bit 5, 0x20) is unaffected by the rebind and by the
--     ripple changes (snap is on in a fresh profile; SNAP stays bound to
--     `action 1157` in the written file).
--
-- The written file also binds ESC.tap = action 40635 (remove time
-- selection, no LED bit): the fake device's single ESC tap proves the
-- device path runs through the written file.
--
-- The harness sources oracle/scenarios/leds_binding_driven.env, which
-- points SPEED_REAPER_FAKE_DEVICE at oracle/fake/leds_binding_driven.txt
-- (device clock). Script timeline (ms since body start; state changes
-- >= 1.5 s apart so each `led main=` line is attributable to its step):
--   0      write bindings.ini (SNAP, CLOSE_UP, ESC) + reload bindings
--   1000   check: only v0 = <snap bit> traced so far, no CLOSE_UP bit
--   1600   Main_OnCommand(40311)  ripple all tracks -> CLOSE_UP bit on
--   3200   Main_OnCommand(40310)  ripple per-track  -> CLOSE_UP bit off
--   5200   finish
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

local CLOSE_UP_BIT = 0x01  -- LED_CLOSE_UP = 0 (speed_editor/key_map.h)

start("device", function()
  record("app_version", reaper.GetAppVersion())
  local off = calibrate()
  check("calibrated trace offset (log connected found)", off ~= nil,
        "off=" .. tostring(off))
  if off == nil then finish() return end

  local snap_bit = reaper.GetToggleCommandState(1157) == 1 and 0x20 or 0x00
  local v0 = snap_bit                 -- ripple off / per-track: CLOSE_UP dark
  local v1 = snap_bit | CLOSE_UP_BIT  -- ripple all: CLOSE_UP lit

  -- Write the rebinding file: CLOSE_UP -> action 40311 (ripple all),
  -- SNAP kept on action 1157, ESC on action 40635 (device-path coverage).
  local path = reaper.GetResourcePath() .. "/speed-reaper/bindings.ini"
  local f = io.open(path, "w")
  check("setup: bindings.ini writable", f ~= nil, path)
  if not f then return finish() end
  f:write("[bindings]\n",
          "SNAP.tap = action 1157\n",
          "CLOSE_UP.tap = action 40311\n",
          "ESC.tap = action 40635\n")
  f:close()
  local reload = reaper.NamedCommandLookup("_SPEED_REAPER_RELOAD_BINDINGS")
  check("reload action registered", reload > 0, "id=" .. tostring(reload))
  reaper.Main_OnCommand(reload, 0)

  local ts1, ts2 = nil, nil
  local done = {}
  local t_start = reaper.time_precise()

  wait_ms(5200, function()
    local st = (reaper.time_precise() - t_start) * 1000

    -- v0 window: every `led main=` line traced before the ripple change
    -- must be the snap-only mask — CLOSE_UP's bit dark while ripple-all
    -- is off, even though the key is now bound to 40311.
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
      check("ripple off: `led main=` traced, CLOSE_UP bit dark, snap-only mask",
            n >= 1 and bad == nil,
            string.format("lines=%d want=%d first_bad=%s snap_bit=%d",
                          n, v0, tostring(bad), snap_bit))
    end

    if not done.s1 and st >= 1600 then
      done.s1 = true; ts1 = st + off
      reaper.Main_OnCommand(40311, 0)  -- Set ripple editing all tracks
    elseif not done.s2 and st >= 3200 then
      done.s2 = true; ts2 = st + off
      reaper.Main_OnCommand(40310, 0)  -- Set ripple editing per-track
    end
  end, function()
    local lines = read_trace()
    if lines then
      -- The device tap through the written file: ESC tap -> cmd 40635.
      local gest, cmd = 0, 0
      for _, l in ipairs(lines) do
        if l:find(" gesture 0031 tap", 1, true) then gest = 1 end
        if l:find(" cmd 40635", 1, true) then cmd = 1 end
      end
      check("device: ESC tap traced via written bindings (cmd 40635)",
            gest == 1 and cmd == 1,
            string.format("gest=%d cmd=%d", gest, cmd))

      -- The written file must have been parsed (n=3: SNAP, CLOSE_UP,
      -- ESC; the embedded default binds far more keys).
      local i_loaded = find_line(lines, "bindings loaded n=3")
      check("written bindings parsed: `bindings loaded n=3` traced",
            i_loaded > 0, "i_loaded=" .. i_loaded)

      -- All `led main=` lines with script time (offset-calibrated);
      -- idx = index into `lines` so lines can be ordered against each other.
      local led = {}
      for idx, l in ipairs(lines) do
        local ms, b = l:match("^(%d+) led main=([0-9a-f]+)$")
        if ms then table.insert(led, { idx = idx, t = tonumber(ms) + off, v = tonumber(b, 16) }) end
      end

      local i1, i0after = 0, 0
      for i = 1, #led do
        if i1 == 0 and led[i].v == v1 then i1 = i end
        if i1 ~= 0 and i0after == 0 and led[i].v == v0 and i > i1 then i0after = i end
      end

      local function within(i, ts, label)
        check(label .. ": `led main=` appears within 1.5 s of the step",
              i ~= 0 and ts ~= nil and led[i].t >= ts - 300 and led[i].t <= ts + 1500,
              string.format("i=%d t=%s step_t=%s", i,
                            i ~= 0 and string.format("%.0f", led[i].t) or "nil",
                            tostring(ts)))
      end
      within(i1, ts1, "40311 ripple all: CLOSE_UP bit lit (mask "
            .. string.format("%x", v1) .. ")")
      within(i0after, ts2, "40310 ripple per-track: CLOSE_UP bit dark again")

      -- The CLOSE_UP bit lit up after the written bindings were parsed —
      -- with the default (CLOSE_UP unbound) it could never light.
      check("CLOSE_UP bit lit only after `bindings loaded n=3`",
            i_loaded > 0 and i1 > 0 and led[i1].idx > i_loaded,
            string.format("i_loaded=%d first_lit_idx=%s", i_loaded,
                          i1 > 0 and led[i1].idx or "nil"))

      -- SNAP's own bit (0x20): set on every traced line — unaffected by
      -- the rebind and by the ripple changes.
      local n, no_snap = #led, nil
      for i = 1, #led do
        if (led[i].v & 0x20) == 0 and no_snap == nil then no_snap = i end
      end
      check("SNAP bit (0x20) set on every `led main=` line (unaffected)",
            n >= 2 and no_snap == nil,
            string.format("lines=%d first_without=%s vals=%s", n,
                          tostring(no_snap),
                          (function()
                            local s = {}
                            for i = 1, math.min(n, 6) do
                              s[#s + 1] = string.format("%x", led[i].v)
                            end
                            return table.concat(s, " ")
                          end)()))

      check("CLOSE_UP bit (0x01) set exactly when ripple-all was on: "
            .. "dark before, lit at 40311, dark again at 40310",
            i1 ~= 0 and i0after ~= 0 and i0after > i1,
            string.format("i1=%d i0after=%d n=%d", i1, i0after, n))
    end

    os.remove(path)  -- later scenarios/runs get the default file again
    finish()
  end)
end)
