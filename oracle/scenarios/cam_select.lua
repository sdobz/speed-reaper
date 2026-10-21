-- oracle/scenarios/cam_select.lua — the CAM `select_track` LED, end to end
-- (vibes/45-cam-n.md task 5; replaces the leds_cam scenario).
--
-- Layout: two tracks, one item each spanning [0,2]; the edit cursor (1.0)
-- overlaps both items. The plugin's ~250 ms LED poll re-reads the
-- bindings and traces `led main=<hex>` on change; every step below
-- asserts its traced value within 1.5 s of the step.
--
-- Phase 1 — default bindings (CAM2.tap = select_track 2), ms since body
-- start (device clock ~= script clock):
--   ~0     setup; baseline: nothing selected -> snap bit only (v0)
--   1200   Lua: B_UISEL=1 on the track-2 item -> v1 = v0 | 0x8000
--           (CAM2 = LED bit 15) lit with NO key press
--   2700   device: CAM2 (0x34) tap -> select_track 2 -> item deselects -> v0
--   4200   device: CAM2 tap again -> item reselects -> v1
--   5700   Lua: edit cursor -> 5.0 (off both items) -> no track-2 item
--           under the cursor -> v0
--   7200   Lua: edit cursor back -> 1.0 -> v1
--
-- Phase 2 — a written bindings.ini (model: bindings.lua) rebinding
-- CLOSE_UP.tap = select_track 2. CLOSE_UP's bit (LED_CLOSE_UP = bit 0,
-- 0x0001) now follows the track-2 state; every other key is unbound in
-- the written file, so the snap bit is dark too:
--   8700   write bindings.ini + "Speed Reaper: reload bindings" -> 0x0001
--           (item selected, cursor on the item)
--   10200  Lua: deselect the track-2 item -> 0x0000
--   11700  Lua: reselect the track-2 item -> 0x0001
--   13200  finish (trace checks)
--
-- The fake device (cam_select.env -> oracle/fake/cam_select.txt) taps
-- only CAM2, at 2700/2750 and 4200/4250.
--
-- Trace lines are "<ms since plugin load> <msg>" (plugin/src/trace.cpp);
-- reaper.time_precise() is a different epoch, so the offset is calibrated
-- on the `log connected` line at body start (both clocks are monotonic
-- and run at the same rate).
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

local SNAP_BIT = 0x20       -- LED_SNAP = 5; snap is on in a fresh profile
local CAM2_BIT = 0x8000     -- LED_CAM2 = 15
local CLOSE_UP_BIT = 0x01   -- LED_CLOSE_UP = 0

start("device", function()
  record("app_version", reaper.GetAppVersion())
  local off = calibrate()
  check("calibrated trace offset (log connected found)", off ~= nil,
        "off=" .. tostring(off))
  if off == nil then finish() return end

  -- Two tracks, one item each spanning [0,2], nothing selected.
  reaper.InsertTrackAtIndex(0, false)
  reaper.InsertTrackAtIndex(1, false)
  local items = {}
  for t = 0, 1 do
    local tr = reaper.GetTrack(nil, t)
    local it = reaper.AddMediaItemToTrack(tr)
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", 0.0)
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 2.0)
    reaper.SetMediaItemInfo_Value(it, "B_UISEL", 0)
    reaper.SetMediaItemInfo_Value(it, "B_SELECTED", 0)
    items[t + 1] = it
  end
  reaper.SetEditCurPos(1.0, false, false)
  reaper.UpdateArrange()

  local snap = reaper.GetToggleCommandState(1157)
  local v0 = snap == 1 and SNAP_BIT or 0x00  -- nothing selected
  local v1 = v0 | CAM2_BIT                   -- track-2 item selected
  local p0 = CLOSE_UP_BIT                    -- phase 2: CLOSE_UP follows track 2
  local p1 = 0x00

  local done = {}
  local reads = {}
  local ts = {}
  local t_start = reaper.time_precise()

  wait_ms(13200, function()
    local st = (reaper.time_precise() - t_start) * 1000

    -- Baseline (before any change): every `led main=` line so far must
    -- be the fresh-profile mask — nothing selected, so no CAM bit.
    if not done.base and st >= 800 then
      done.base = true
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
      check("baseline: `led main=` traced, nothing selected (no CAM bit)",
            n >= 1 and bad == nil,
            string.format("lines=%d want=%d first_bad=%s snap=%s",
                          n, v0, tostring(bad), tostring(snap)))
    end

    -- s1: select the track-2 item via B_UISEL — no key press.
    if not done.s1 and st >= 1200 then
      done.s1 = true; ts.s1 = st + off
      reaper.SetMediaItemInfo_Value(items[2], "B_UISEL", 1)
      reaper.UpdateArrange()
    elseif not done.chk1 and st >= 3400 then
      -- pure state check in the idle gap (reads change nothing)
      done.chk1 = true
      reads.sel_after_tap1 = reaper.IsMediaItemSelected(items[2])
      reads.ui_after_tap1 = reaper.GetMediaItemInfo_Value(items[2], "B_UISEL")
      reads.sel1_after_tap1 = reaper.IsMediaItemSelected(items[1])
    elseif not done.s4 and st >= 5700 then
      done.s4 = true; ts.s4 = st + off
      reads.sel_after_tap2 = reaper.IsMediaItemSelected(items[2])
      reaper.SetEditCurPos(5.0, false, false)   -- off both items
      reaper.UpdateArrange()
    elseif not done.s5 and st >= 7200 then
      done.s5 = true; ts.s5 = st + off
      reaper.SetEditCurPos(1.0, false, false)   -- back on the items
      reaper.UpdateArrange()
    elseif not done.s6 and st >= 8700 then
      done.s6 = true; ts.s6 = st + off
      -- Phase 2: written bindings.ini rebinding CLOSE_UP to select_track 2.
      local path = reaper.GetResourcePath() .. "/speed-reaper/bindings.ini"
      local f = io.open(path, "w")
      check("phase 2: bindings.ini writable", f ~= nil, path)
      if f then
        f:write("[bindings]\n", "CLOSE_UP.tap = select_track 2\n")
        f:close()
        reads.bindings_path = path
        local reload = reaper.NamedCommandLookup("_SPEED_REAPER_RELOAD_BINDINGS")
        check("phase 2: reload action registered", reload > 0,
              "id=" .. tostring(reload))
        reaper.Main_OnCommand(reload, 0)
      end
    elseif not done.s7 and st >= 10200 then
      done.s7 = true; ts.s7 = st + off
      reaper.SetMediaItemInfo_Value(items[2], "B_UISEL", 0)
      reaper.UpdateArrange()
    elseif not done.s8 and st >= 11700 then
      done.s8 = true; ts.s8 = st + off
      reaper.SetMediaItemInfo_Value(items[2], "B_UISEL", 1)
      reaper.UpdateArrange()
    end
  end, function()
    if not done.s1 then
      check("timeline completed", false,
            "steps done: base=" .. tostring(done.base) .. " s1=" ..
            tostring(done.s1))
      return finish()
    end

    check("tap 1 (device CAM2) deselected the track-2 item",
          reads.sel_after_tap1 == false,
          string.format("IsSel=%s B_UISEL=%s", tostring(reads.sel_after_tap1),
                        tostring(reads.ui_after_tap1)))
    check("tap 2 (device CAM2) reselected the track-2 item",
          reads.sel_after_tap2 == true,
          "IsSel=" .. tostring(reads.sel_after_tap2))
    check("track-1 item untouched by the track-2 toggles",
          reads.sel1_after_tap1 == false and
          reaper.IsMediaItemSelected(items[1]) == false,
          string.format("sel=%s now=%s", tostring(reads.sel1_after_tap1),
                        tostring(reaper.IsMediaItemSelected(items[1]))))

    local lines = read_trace()
    if lines then
      -- Device path: exactly two CAM2 taps, and none before the first
      -- (proof that the s1 CAM2 bit lit with no key press).
      local gest, first_any = {}, nil
      for _, l in ipairs(lines) do
        local ms, key = l:match("^(%d+) gesture (%x+) tap$")
        if ms then
          local t = tonumber(ms) + off
          if first_any == nil or t < first_any then first_any = t end
          if key == "0034" then table.insert(gest, t) end
        end
      end
      check("device: exactly two CAM2 (0x34) taps traced",
            #gest == 2, "[" .. table.concat(gest, ", ") .. "]")
      check("device: no key press before the first CAM2 tap (s1 lit with no press)",
            first_any == nil or first_any >= 2600 + off,
            "first_gesture=" .. tostring(first_any))

      check("phase 2: bindings loaded n=1",
            find_line(lines, "bindings loaded n=1") > 0, "")

      -- All `led main=` lines with script time (offset-calibrated).
      local led = {}
      for _, l in ipairs(lines) do
        local ms, hx = l:match("^(%d+) led main=([0-9a-f]+)$")
        if ms then
          table.insert(led, { t = tonumber(ms) + off, v = tonumber(hx, 16) })
        end
      end

      -- First line with value `v` at/after script time `from`.
      local function first_val(v, from)
        for i = 1, #led do
          if led[i].t >= from and led[i].v == v then return i end
        end
        return 0
      end
      local function within(i, ts_step, v, label)
        check(label .. ": `led main=`=" .. string.format("%x", v) ..
              " within 1.5 s of the step",
              i ~= 0 and led[i].t >= ts_step - 100 and led[i].t <= ts_step + 1500,
              string.format("i=%d t=%s step_t=%s", i,
                            i ~= 0 and string.format("%.0f", led[i].t) or "nil",
                            string.format("%.0f", ts_step)))
      end

      local ia = first_val(v1, ts.s1)       -- B_UISEL set, no key press
      local ib = first_val(v0, 2700 + off)  -- tap 1 deselects
      local ic = first_val(v1, 4200 + off)  -- tap 2 reselects
      local id = first_val(v0, ts.s4)       -- cursor off the item
      local ie = first_val(v1, ts.s5)       -- cursor back on the item
      local if_ = first_val(p0, ts.s6)      -- reload: CLOSE_UP follows track 2
      local ig = first_val(p1, ts.s7)       -- deselect
      local ih = first_val(p0, ts.s8)       -- reselect

      within(ia, ts.s1, v1, "s1: B_UISEL set on track-2 item (no key press): CAM2 lit")
      within(ib, 2700 + off, v0, "s2: CAM2 tap deselects the item: CAM2 dark")
      within(ic, 4200 + off, v1, "s3: CAM2 tap reselects the item: CAM2 lit")
      within(id, ts.s4, v0, "s4: edit cursor off the item: CAM2 dark")
      within(ie, ts.s5, v1, "s5: edit cursor back on the item: CAM2 lit")
      within(if_, ts.s6, p0, "s6: bindings.ini CLOSE_UP=select_track 2 + reload: CLOSE_UP lit")
      within(ig, ts.s7, p1, "s7: track-2 item deselected: CLOSE_UP dark")
      within(ih, ts.s8, p0, "s8: track-2 item reselected: CLOSE_UP lit")

      check("order: all eight `led main=` lines appear in step order",
            ia > 0 and ib > ia and ic > ib and id > ic and ie > id and
            if_ > ie and ig > if_ and ih > ig,
            string.format("i=[%d %d %d %d %d %d %d %d] n=%d",
                          ia, ib, ic, id, ie, if_, ig, ih, #led))
    end

    if reads.bindings_path then os.remove(reads.bindings_path) end
    finish()
  end)
end)
