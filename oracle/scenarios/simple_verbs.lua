-- oracle/scenarios/simple_verbs.lua — M6 simple verbs end-to-end
-- (vibes/32-m6-simple-verbs.md task 6). Validates the M6 verb rows in
-- plugin/src/verbs.cpp against REAPER-visible effects.
--
-- The harness sources oracle/scenarios/simple_verbs.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/simple_verbs.txt (device clock;
-- the CLI script starts ~40 ms after plugin load, so script time = device
-- time - ~40 ms, same offset the verbs scenario relies on):
--   ~410         CAM 2 tap 0x34            -> I_CURTAKE=1 on target A
--   ~910         AUDIO LEVEL 0x2C x2       -> cmd 40157, marker at cursor 1
--   ~1410        PLACE ON TOP 0x05         -> A duplicated onto new track (C)
--   ~1910        SMTH CUT 0x11             -> cmd 40916 within time sel [1,2]
--   ~2310/2560   AUDIO_ONLY 0x26 x2        -> solo set then restored
--   ~2960/3260   VIDEO_ONLY 0x25 x2        -> tracks hidden then restored
--   ~3810        FULL VIEW 0x2D            -> cmd 40346 (toggle fullscreen)
--
-- Setup: track 1 "A" with item A [0,5] holding TWO takes, track 2 "B" with
-- item B [0,5] one take, nothing selected, edit cursor at 1 s, last-touched
-- track = A's track (so the implicit-target rule resolves A with no
-- selection). PLACE ON TOP runs before SMTH CUT so the time selection [1,2]
-- overlaps both A and C (A's copy) at the cut.
--
-- The time selection [1,2] is set by THIS script (no Speed Editor button
-- sets an arbitrary range); SMTH CUT only needs it to exist.
dofile(os.getenv("ORACLE_LIB"))

local function track_name(tr)
  local buf = string.rep("\0", 128)
  reaper.GetSetMediaTrackInfo_String(tr, "NAME", buf, #buf, false)
  return buf:match("^([^\0]*)") or ""
end

start("device", function()
  record("app_version", reaper.GetAppVersion())

  -- Setup: two tracks with named items, cursor at 1 s, no selection.
  reaper.InsertTrackAtIndex(0, true) -- bool; track is index 0
  local trackA = reaper.GetTrack(nil, 0)
  reaper.InsertTrackAtIndex(1, true)
  local trackB = reaper.GetTrack(nil, 1)
  -- Track tracks are userdata in REAPER Lua (no .name property): name them
  -- via GetSetMediaTrackInfo_String (set form; SetMediaTrackInfo_String is
  -- not a Lua API).
  reaper.GetSetMediaTrackInfo_String(trackA, "NAME", "A", 1, true)
  reaper.GetSetMediaTrackInfo_String(trackB, "NAME", "B", 1, true)
  local itemA = reaper.AddMediaItemToTrack(trackA)
  reaper.SetMediaItemInfo_Value(itemA, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(itemA, "D_LENGTH", 5.0)
  reaper.AddTakeToMediaItem(itemA, true)  -- items start with 0 takes
  reaper.AddTakeToMediaItem(itemA, true)  -- A: two takes
  local itemB = reaper.AddMediaItemToTrack(trackB)
  reaper.SetMediaItemInfo_Value(itemB, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(itemB, "D_LENGTH", 5.0)
  reaper.AddTakeToMediaItem(itemB, true)  -- B: one take
  -- Implicit target with no item selection: TargetTrack is the first
  -- selected track, else the last touched one. Select A's track (SetTrack-
  -- LastTouched is not in the REAPER Lua API) and pin it as last touched via
  -- 40914 "Track: Set first selected track as last touched track".
  reaper.SetMediaTrackInfo_Value(trackA, "B_SELECTED", 1)
  reaper.Main_OnCommand(40914, 0)
  reaper.SetEditCurPos(1.0, false, false)
  reaper.UpdateArrange()

  check("setup: no item selected",
        reaper.GetSelectedMediaItem(nil, 0) == nil, "")

  check("setup: two tracks", reaper.CountTracks(nil) == 2,
        "tracks=" .. reaper.CountTracks(nil))
  check("setup: A has 2 takes, B has 1",
        reaper.CountTakes(itemA) == 2 and reaper.CountTakes(itemB) == 1,
        string.format("A=%d B=%d", reaper.CountTakes(itemA),
                      reaper.CountTakes(itemB)))
  check("setup: edit cursor at 1 s",
        math.abs(reaper.GetCursorPosition() - 1.0) < 1e-6,
        "GetCursorPosition=" .. tostring(reaper.GetCursorPosition()))

  -- Snapshot every defer cycle for ~4.7 s (last effect lands ~3.81 s in).
  local samples = {} -- {t, tracks, items, solo, bshowB, maxauto, markers}
  local t0 = reaper.time_precise()
  local tsel_set = false

  wait_ms(4700, function()
    local st = (reaper.time_precise() - t0) * 1000
    -- Time selection [1,2] once after PLACE ON TOP, before SMTH CUT.
    if not tsel_set and st > 1500 and st < 1850 then
      tsel_set = true
      reaper.GetSet_LoopTimeRange(true, true, 1.0, 2.0, true)
    end
    local s = { t = st, tracks = reaper.CountTracks(nil), items = 0,
                solo = 0, bshowB = nil, maxauto = 0,
                markers = reaper.EnumProjectMarkers(0) }
    for t = 0, s.tracks - 1 do
      local tr = reaper.GetTrack(nil, t)
      s.solo = s.solo + reaper.GetMediaTrackInfo_Value(tr, "I_SOLO")
      if track_name(tr):find("B") then
        s.bshowB = reaper.GetMediaTrackInfo_Value(tr, "B_SHOWINTCP")
      end
      local n = reaper.CountMediaItems(nil, tr)
      s.items = s.items + n
      for i = 0, n - 1 do
        local it = reaper.GetTrackMediaItem(tr, i)
        if it then
          local fa = reaper.GetMediaItemInfo_Value(it, "D_FADEINLEN_AUTO")
          if fa > s.maxauto then s.maxauto = fa end
        end
      end
    end
    table.insert(samples, s)
  end, function()
    local n = #samples
    if n < 2 then
      check("samples collected", false, "n=" .. n)
      finish()
      return
    end

    -- One mid-window sample per step (script clock; device - ~40 ms).
    local function mid(lo, hi)
      for i = 1, n do
        if samples[i].t >= lo and samples[i].t <= hi then return i end
      end
      return 0
    end
    local function at(i, k) return samples[i] and samples[i][k] or nil end

    local function track_by_name(name)
      for t = 0, reaper.CountTracks(nil) - 1 do
        local tr = reaper.GetTrack(nil, t)
        if track_name(tr):find(name) then return t, tr end
      end
      return -1, nil
    end

    -- CAM 2 (I_CURTAKE = 1 on target A; B keeps take 0). Sampled before
    -- PLACE ON TOP, when A is on its original track.
    local i1 = mid(500, 800)
    -- Track names survive PLACE ON TOP (only an UNNAMED copy track is
    -- inserted), so A and B are found by name at assertion time; all of
    -- A's post-SMTH-CUT pieces share its I_CURTAKE.
    local _, a_track = track_by_name("A")
    local _, b_track = track_by_name("B")
    local itemA0 = a_track and reaper.GetTrackMediaItem(a_track, 0)
    local itemB0 = b_track and reaper.GetTrackMediaItem(b_track, 0)
    local curta, curtb = nil, nil
    if itemA0 then curta = reaper.GetMediaItemInfo_Value(itemA0, "I_CURTAKE") end
    if itemB0 then curtb = reaper.GetMediaItemInfo_Value(itemB0, "I_CURTAKE") end
    check("CAM 2: A's I_CURTAKE == 1 (2-take item, pre-duplicate)",
          i1 ~= 0 and curta == 1,
          string.format("i1=%d curta=%s", i1, tostring(curta)))
    check("CAM 2: B's I_CURTAKE still 0",
          i1 ~= 0 and curtb == 0,
          string.format("i1=%d curtb=%s", i1, tostring(curtb)))

    -- AUDIO LEVEL double-tap (cmd 40157 ~910 ms): marker at cursor 1.
    local mp = mid(500, 800)  -- before the double-tap
    local ma = mid(1050, 1350)  -- after it, before PLACE ON TOP
    local mpos = {}
    local mcount = at(ma, "markers") or 0
    for i = 1, mcount do
      local _, idx, pos = reaper.EnumProjectMarkers(i)
      table.insert(mpos, pos)
    end
    local at_cursor
    for _, p in ipairs(mpos) do
      if p and math.abs(p - 1.0) < 0.1 then at_cursor = true end
    end
    check("AUDIO LEVEL x2: marker count +1 at cursor 1",
          mp ~= 0 and ma ~= 0
              and at(ma, "markers") == at(mp, "markers") + 1
              and at_cursor,
          string.format("before=%s after=%s pos=%s",
                        tostring(at(mp, "markers")), tostring(mcount),
                        table.concat(mpos, ",")))

    -- PLACE ON TOP (~1410 ms): track count +1 and item count +1.
    local ip1 = mid(1200, 1350)
    local ip2 = mid(1500, 1800)
    check("PLACE ON TOP: track count +1",
          ip1 ~= 0 and ip2 ~= 0
              and at(ip2, "tracks") == at(ip1, "tracks") + 1,
          string.format("before=%s after=%s",
                        tostring(at(ip1, "tracks")),
                        tostring(at(ip2, "tracks"))))
    check("PLACE ON TOP: item count +1",
          ip1 ~= 0 and ip2 ~= 0
              and at(ip2, "items") == at(ip1, "items") + 1,
          string.format("before=%s after=%s",
                        tostring(at(ip1, "items")),
                        tostring(at(ip2, "items"))))

    -- SMTH CUT (cmd 40916 ~1910 ms) within [1,2] over overlapping A/C:
    -- the build produces a fade — assert D_FADEINLEN_AUTO > 0 on some
    -- item and record what was measured.
    local ic = mid(2000, 2250)
    local cf_before = at(ip2, "maxauto") or 0
    local cf_after = at(ic, "maxauto") or 0
    check("SMTH CUT: D_FADEINLEN_AUTO > 0 after cut in [1,2]",
          ic ~= 0 and cf_after > 0,
          string.format("before=%s after=%s",
                        tostring(cf_before), tostring(cf_after)))
    record("smth_cut_max_fadein_auto", string.format("%g", cf_after))

    -- AUDIO_ONLY x2 (~2310/~2560 ms): solo set then cleared.
    local is0 = mid(2100, 2250)
    local is1 = mid(2350, 2500)
    local is2 = mid(2650, 2900)
    check("AUDIO_ONLY: solo flag set then cleared",
          is0 ~= 0 and is1 ~= 0 and is2 ~= 0
              and at(is0, "solo") == 0 and at(is1, "solo") == 1
              and at(is2, "solo") == 0,
          string.format("before=%s mid=%s after=%s",
                        tostring(at(is0, "solo")),
                        tostring(at(is1, "solo")),
                        tostring(at(is2, "solo"))))

    -- VIDEO_ONLY x2 (~2960/~3260 ms): track B hidden then shown again
    -- (isolate hides every track except the target's).
    local iv0 = mid(2700, 2900)
    local iv1 = mid(3050, 3200)
    local iv2 = mid(3350, 3700)
    check("VIDEO_ONLY: track B hidden then shown",
          iv0 ~= 0 and iv1 ~= 0 and iv2 ~= 0
              and at(iv0, "bshowB") == 1 and at(iv1, "bshowB") == 0
              and at(iv2, "bshowB") == 1,
          string.format("before=%s mid=%s after=%s",
                        tostring(at(iv0, "bshowB")),
                        tostring(at(iv1, "bshowB")),
                        tostring(at(iv2, "bshowB"))))

    -- FULL VIEW (~3810 ms): the command exists (toggle state, not -1).
    local iv3 = mid(3900, 4500)
    local fv = reaper.GetToggleCommandState(40346)
    check("FULL VIEW: 40346 exists (GetToggleCommandState ~= -1)",
          iv3 ~= 0 and fv ~= -1,
          string.format("toggle_state=%s", tostring(fv)))

    -- Trace checks (build/oracle/trace_simple_verbs.txt, "<ms> <msg>").
    local lines = read_trace()
    if lines then
      -- 1) The 9 gesture lines, in script order. Composites and id rows
      --    trace the gesture; only SMTH CUT (40916) and FULL VIEW (40346)
      --    also trace a cmd line.
      local gest = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) ([a-z_]+)$")
        if k then table.insert(gest, { k, kind }) end
      end
      local want = { { "0034", "tap" }, { "002c", "double_tap" },
                     { "0005", "tap" }, { "0011", "tap" },
                     { "0026", "tap" }, { "0026", "tap" },
                     { "0025", "tap" }, { "0025", "tap" },
                     { "002d", "tap" } }
      local gest_ok = #gest == #want
      for i = 1, #want do
        if not (gest[i] and gest[i][1] == want[i][1]
                and gest[i][2] == want[i][2]) then gest_ok = false end
      end
      local gest_dump = ""
      for i = 1, math.min(#gest, 12) do
        gest_dump = gest_dump .. gest[i][1] .. "/" .. gest[i][2] .. " "
      end
      check("trace: 9 gestures in script order (CAM2, AUDL x2, POT, SMTH, "
            .. "AUDONLY x2, VIDONLY x2, FULL)",
            gest_ok, string.format("n=%d [%s]", #gest, gest_dump))

      -- 2) The cmd lines: exactly 40916 then 40346, in order.
      local cmds = {}
      for _, l in ipairs(lines) do
        local c = l:match(" cmd (%d+)$")
        if c then table.insert(cmds, tonumber(c)) end
      end
      local want_cmds = { 40916, 40346 }
      local cmd_ok = #cmds == #want_cmds
      for i = 1, #want_cmds do
        if cmds[i] ~= want_cmds[i] then cmd_ok = false end
      end
      local cmd_dump = ""
      for _, c in ipairs(cmds) do cmd_dump = cmd_dump .. c .. " " end
      check("trace: cmd lines are 40916 then 40346 (SMTH CUT, FULL VIEW)",
            cmd_ok, string.format("n=%d [%s]", #cmds, cmd_dump))

      -- 3) FULL VIEW's `cmd 40346` line is present (the task's trace check).
      check("trace: `cmd 40346` traced for FULL VIEW",
            find_line(lines, "cmd 40346") > 0, "")
    end

    finish()
  end)
end)
