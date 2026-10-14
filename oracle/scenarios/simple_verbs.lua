-- oracle/scenarios/simple_verbs.lua — M6 simple verbs (vibes/32-m6-simple-verbs.md
-- task 6a). Validates the three simple verb rows in plugin/src/verbs.cpp
-- against REAPER-visible effects. The other M6 verbs (PLACE ON TOP, SMTH
-- CUT, AUDIO_ONLY, VIDEO_ONLY) have their own scenarios.
--
-- The harness sources oracle/scenarios/simple_verbs.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/simple_verbs.txt (device clock;
-- the CLI script starts ~40 ms after plugin load, so script time = device
-- time - ~40 ms, same offset the verbs scenario relies on):
--   ~410         CAM 2 tap 0x34            -> I_CURTAKE=1 on target A
--   ~910         AUDIO LEVEL 0x2C x2       -> cmd 40157, marker at cursor 1
--   ~1410        FULL VIEW 0x2D            -> cmd 40346 (toggle fullscreen)
--
-- Setup: track 1 "A" with item A [0,5] holding TWO takes, track 2 "B" with
-- item B [0,5] one take, nothing selected, edit cursor at 1 s, last-touched
-- track = A's track (so the implicit-target rule resolves A with no
-- selection). No tone sources needed: these three verbs never touch item
-- audio (unlike SMTH CUT, which has its own scenario).
dofile(os.getenv("ORACLE_LIB"))

-- M6 findings (docs/notes/m5-observations.md ## M6): the REAPER 7.x param
-- is "P_NAME" ("NAME" silently fails) and the Lua binding is 4-arg (no
-- buf_len).
local function track_name(tr)
  local _, buf = reaper.GetSetMediaTrackInfo_String(tr, "P_NAME", "", false)
  return buf or ""
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
  reaper.GetSetMediaTrackInfo_String(trackA, "P_NAME", "A", true)
  reaper.GetSetMediaTrackInfo_String(trackB, "P_NAME", "B", true)
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

  -- Snapshot every defer cycle for ~2 s (last effect lands ~1.45 s in).
  local samples = {} -- {t, markers, curta, curtb}
  local t0 = reaper.time_precise()

  wait_ms(2000, function()
    local st = (reaper.time_precise() - t0) * 1000
    -- M6: EnumProjectMarkers(0)'s first return is retval, not a count;
    -- count markers with GetNumRegionsOrMarkers(nil).
    local t0tr = reaper.GetTrack(nil, 0)
    local t1tr = reaper.GetTrack(nil, 1)
    local i0 = t0tr and reaper.GetTrackMediaItem(t0tr, 0)
    local i1 = t1tr and reaper.GetTrackMediaItem(t1tr, 0)
    table.insert(samples, {
      t = st,
      markers = reaper.GetNumRegionsOrMarkers(nil),
      curta = i0 and reaper.GetMediaItemInfo_Value(i0, "I_CURTAKE") or nil,
      curtb = i1 and reaper.GetMediaItemInfo_Value(i1, "I_CURTAKE") or nil,
    })
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

    -- CAM 2 (~450 ms): I_CURTAKE = 1 on target A (2-take item), B keeps
    -- take 0. Sampled after the tap, before the double-tap resolves.
    local i1 = mid(500, 750)
    check("CAM 2: A's I_CURTAKE == 1 (2-take item)",
          i1 ~= 0 and at(i1, "curta") == 1,
          string.format("i1=%d curta=%s", i1, tostring(at(i1, "curta"))))
    check("CAM 2: B's I_CURTAKE still 0",
          i1 ~= 0 and at(i1, "curtb") == 0,
          string.format("i1=%d curtb=%s", i1, tostring(at(i1, "curtb"))))
    -- Final state: still 1 / 0 after all gestures (none of the other two
    -- verbs touches takes).
    check("final: A's I_CURTAKE == 1, B's 0",
          at(n, "curta") == 1 and at(n, "curtb") == 0,
          string.format("curta=%s curtb=%s", tostring(at(n, "curta")),
                        tostring(at(n, "curtb"))))

    -- AUDIO LEVEL double-tap (cmd 40157 ~910 ms): marker count +1, at
    -- cursor 1. M6: EnumProjectMarkers is 0-based; return order is
    -- (retval, isrgn, pos, rgnend, name, markrgnindexnumber), so pos is
    -- return #3.
    local mp = mid(500, 750)   -- before the double-tap resolves
    local ma = mid(1050, 1300) -- after it, before FULL VIEW
    check("AUDIO LEVEL x2: marker count +1",
          mp ~= 0 and ma ~= 0
              and at(ma, "markers") == at(mp, "markers") + 1,
          string.format("before=%s after=%s",
                        tostring(at(mp, "markers")),
                        tostring(at(ma, "markers"))))
    local at_cursor
    local mcount = reaper.GetNumRegionsOrMarkers(nil)
    for i = 0, mcount - 1 do
      local _, _, pos = reaper.EnumProjectMarkers(i)
      if pos and math.abs(pos - 1.0) < 0.1 then at_cursor = true end
    end
    check("AUDIO LEVEL x2: marker at cursor 1",
          at_cursor ~= nil,
          string.format("count=%d", mcount))

    -- FULL VIEW (~1450 ms): the command exists (toggle state, not -1).
    local iv = mid(1500, 1900)
    local fv = reaper.GetToggleCommandState(40346)
    check("FULL VIEW: 40346 exists (GetToggleCommandState ~= -1)",
          iv ~= 0 and fv ~= -1,
          string.format("toggle_state=%s", tostring(fv)))

    -- Trace checks (build/oracle/trace_simple_verbs.txt, "<ms> <msg>").
    local lines = read_trace()
    if lines then
      -- 1) The 3 gesture lines, in script order.
      local gest = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) ([a-z_]+)$")
        if k then table.insert(gest, { k, kind }) end
      end
      local want = { { "0034", "tap" }, { "002c", "double_tap" },
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
      check("trace: gestures are CAM2 tap, AUDL double-tap, FULL tap",
            gest_ok, string.format("n=%d [%s]", #gest, gest_dump))

      -- 2) The cmd lines: 40157 (AUDIO LEVEL double-tap) then 40346
      --    (FULL VIEW), in script order.
      local cmds = {}
      for _, l in ipairs(lines) do
        local c = l:match(" cmd (%d+)$")
        if c then table.insert(cmds, tonumber(c)) end
      end
      local want_cmds = { 40157, 40346 }
      local cmd_ok = #cmds == #want_cmds
      for i = 1, #want_cmds do
        if cmds[i] ~= want_cmds[i] then cmd_ok = false end
      end
      local cmd_dump = ""
      for _, c in ipairs(cmds) do cmd_dump = cmd_dump .. c .. " " end
      check("trace: cmd lines are 40157, 40346 (AUDL, FULL)",
            cmd_ok, string.format("n=%d [%s]", #cmds, cmd_dump))
    end

    finish()
  end)
end)
