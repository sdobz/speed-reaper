-- oracle/scenarios/view_solo.lua — M6 view verbs (vibes/32-m6-simple-verbs.md
-- task 6d). Validates AUDIO_ONLY (0x26) and VIDEO_ONLY (0x25) tap-twice:
-- the first tap solos / isolates the target items' tracks, the second tap
-- restores the saved flags (plugin memory only, no undo block, no REAPER
-- command — see the IsolateView / SoloView ops in
-- plugin/src/project_ops.h, on the IProject track-flag primitives).
--
-- The harness sources oracle/scenarios/view_solo.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/view_solo.txt (device clock; the
-- CLI script starts ~40 ms after plugin load, so script time = device
-- time - ~40 ms). Consecutive taps are 500 ms apart (wider than the 300 ms
-- double_tap window), so each pair is two independent taps:
--   ~410         AUDIO_ONLY 0x26 tap 1  -> I_SOLO=1 on the target track(s)
--   ~910         AUDIO_ONLY 0x26 tap 2  -> saved I_SOLO flags restored
--   ~1410        VIDEO_ONLY 0x25 tap 1  -> B_SHOWINTCP=0 on non-targets
--   ~1910        VIDEO_ONLY 0x25 tap 2  -> saved visibility restored
--
-- Setup (commands as in target.lua): two tracks, one item each [0,5];
-- item A selected (B_UISEL — the plugin's TargetItems rule 1, and B_UISEL
-- writes work where B_SELECTED track writes do not in this build) with the
-- edit cursor at 2.5 s (cursor-item fallback, rule 2). The assertions are
-- independent of WHICH track ends up the target:
--   AUDIO_ONLY: sum of I_SOLO over tracks is 0 -> >=1 -> 0.
--   VIDEO_ONLY: number of tracks with B_SHOWINTCP==1 goes N -> fewer -> N.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  reaper.Main_OnCommand(40635, 0) -- no time selection
  reaper.InsertTrackAtIndex(0, true) -- bool; track is index 0
  local trackA = reaper.GetTrack(nil, 0)
  reaper.InsertTrackAtIndex(1, true)
  local trackB = reaper.GetTrack(nil, 1)
  reaper.GetSetMediaTrackInfo_String(trackA, "P_NAME", "A", true)
  reaper.GetSetMediaTrackInfo_String(trackB, "P_NAME", "B", true)
  local itemA = reaper.AddMediaItemToTrack(trackA)
  reaper.SetMediaItemInfo_Value(itemA, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(itemA, "D_LENGTH", 5.0)
  local itemB = reaper.AddMediaItemToTrack(trackB)
  reaper.SetMediaItemInfo_Value(itemB, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(itemB, "D_LENGTH", 5.0)
  -- Target: item A selected (B_UISEL is the reliable item-selection write
  -- in this build; TargetItems rule 1 → its track). Cursor at 2.5 s sits
  -- inside both items as the rule-2 fallback.
  reaper.SetMediaItemSelected(itemA, true)
  reaper.SetMediaItemSelected(itemB, false)
  reaper.SetEditCurPos(2.5, false, false)
  reaper.UpdateArrange()

  local function snap()
    local solo, show = 0, 0
    local n = reaper.CountTracks(nil)
    for i = 0, n - 1 do
      local tr = reaper.GetTrack(nil, i)
      if reaper.GetMediaTrackInfo_Value(tr, "I_SOLO") ~= 0 then solo = solo + 1 end
      if reaper.GetMediaTrackInfo_Value(tr, "B_SHOWINTCP") ~= 0 then show = show + 1 end
    end
    return { n = n, solo = solo, show = show }
  end

  local setup = snap()
  check("setup: two tracks", setup.n == 2,
        "tracks=" .. setup.n)
  check("setup: no track soloed", setup.solo == 0,
        "solo=" .. setup.solo)
  check("setup: all tracks visible in TCP", setup.show == setup.n,
        string.format("show=%d n=%d", setup.show, setup.n))
  check("setup: item A selected (target)",
        reaper.GetMediaItemInfo_Value(itemA, "B_UISEL") ~= 0, "")
  check("setup: item B not selected",
        reaper.GetMediaItemInfo_Value(itemB, "B_UISEL") == 0, "")

  -- Sample every defer cycle for ~2.4 s (last effect: VIDEO_ONLY tap 2
  -- lands ~1910 ms in).
  local samples = {}
  local t0 = reaper.time_precise()

  wait_ms(2400, function()
    local s = snap()
    s.t = (reaper.time_precise() - t0) * 1000
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
        if samples[i].t >= lo and samples[i].t <= hi then return samples[i] end
      end
      return nil
    end

    local s0 = mid(0, 350)     -- before AUDIO_ONLY tap 1 (~410)
    local s1 = mid(550, 800)   -- after tap 1, before tap 2 (~910)
    local s2 = mid(1050, 1300) -- after tap 2, before VIDEO_ONLY (~1410)
    local s3 = mid(1550, 1800) -- after VIDEO_ONLY tap 1, before tap 2 (~1910)
    local s4 = mid(2000, 2300) -- after tap 2

    -- AUDIO_ONLY: sum of I_SOLO over tracks is 0 -> >=1 -> 0.
    check("AUDIO_ONLY tap 1: sum(I_SOLO) 0 -> >=1",
          s0 ~= nil and s1 ~= nil and s0.solo == 0 and s1.solo >= 1,
          string.format("s0=%s solo0=%s solo1=%s",
                        tostring(s0 and s0.t), tostring(s0 and s0.solo),
                        tostring(s1 and s1.solo)))
    check("AUDIO_ONLY tap 2: sum(I_SOLO) restored to 0",
          s2 ~= nil and s2.solo == 0,
          string.format("t=%s solo=%s", tostring(s2 and s2.t),
                        tostring(s2 and s2.solo)))

    -- VIDEO_ONLY: tracks with B_SHOWINTCP==1 go N -> fewer -> N.
    check("VIDEO_ONLY tap 1: B_SHOWINTCP==1 tracks N -> fewer",
          s3 ~= nil and s3.show < setup.n,
          string.format("t=%s show=%s n=%d", tostring(s3 and s3.t),
                        tostring(s3 and s3.show), setup.n))
    check("VIDEO_ONLY tap 2: B_SHOWINTCP==1 tracks back to N",
          s4 ~= nil and s4.show == setup.n,
          string.format("t=%s show=%s n=%d", tostring(s4 and s4.t),
                        tostring(s4 and s4.show), setup.n))

    -- Neither verb ran a REAPER command; the restored states must also
    -- hold at the final sample (nothing after ~1910 ms touches them).
    check("final: no solo, all tracks visible",
          samples[n].solo == 0 and samples[n].show == setup.n,
          string.format("solo=%d show=%d", samples[n].solo, samples[n].show))

    -- Trace checks (build/oracle/trace_view_solo.txt, "<ms> <msg>").
    local lines = read_trace()
    if lines then
      -- 1) The 4 gesture lines, in script order: two taps each, no
      --    double_tap (taps are 500 ms apart, outside the 300 ms window).
      local gest = {}
      for _, l in ipairs(lines) do
        local k, kind = l:match(" gesture ([0-9a-f]+) ([a-z_]+)$")
        if k then table.insert(gest, { k, kind }) end
      end
      local want = { { "0026", "tap" }, { "0026", "tap" },
                     { "0025", "tap" }, { "0025", "tap" } }
      local gest_ok = #gest == #want
      for i = 1, #want do
        if not (gest[i] and gest[i][1] == want[i][1]
                and gest[i][2] == want[i][2]) then gest_ok = false end
      end
      local gest_dump = ""
      for i = 1, #gest do
        gest_dump = gest_dump .. gest[i][1] .. "/" .. gest[i][2] .. " "
      end
      check("trace: 4 taps in order (26,26,25,25), no double_tap",
            gest_ok, string.format("n=%d [%s]", #gest, gest_dump))

      -- 2) No REAPER command ran (both verbs use the project API).
      local cmds = 0
      for _, l in ipairs(lines) do
        if l:match(" cmd (%d+)$") then cmds = cmds + 1 end
      end
      check("trace: no cmd lines (no REAPER command for either verb)",
            cmds == 0, "count=" .. cmds)

      -- 3) No `target none` — both verbs had the selected item's track.
      local none = 0
      for _, l in ipairs(lines) do
        if l:find("target none", 1, true) then none = none + 1 end
      end
      check("trace: no `target none`", none == 0, "count=" .. none)
    end

    finish()
  end)
end)
