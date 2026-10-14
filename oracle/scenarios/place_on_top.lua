-- oracle/scenarios/place_on_top.lua — M6 PLACE ON TOP (vibes/32-m6-simple-verbs.md
-- task 6c). One gesture: PLACE ON TOP 0x05 tap (fires at release, ~410 ms
-- device time). The verb duplicates the target item onto a new track above
-- its source track at the same position (verbs.cpp PlaceOnTopTap ->
-- IProject::DuplicateItemsAbove; no REAPER command id, so no cmd line in
-- the trace).
--
-- The harness sources oracle/scenarios/place_on_top.env, which points
-- SPEED_REAPER_FAKE_DEVICE at oracle/fake/place_on_top.txt (device clock;
-- the CLI script starts ~40 ms after plugin load, so the tap fires ~370 ms
-- into script time):
--   400/450      PLACE ON TOP 0x05 tap
--                -> one new track above the source, copy of A [0,5] on it
--
-- Setup: the target setup from oracle/scenarios/target.lua — one track
-- with adjacent items A [0,5] and B [5,10], nothing selected, edit cursor
-- at 2.5 s (so A is the implicit target), ripple off, no time selection.
dofile(os.getenv("ORACLE_LIB"))

start("device", function()
  record("app_version", reaper.GetAppVersion())

  reaper.Main_OnCommand(40309, 0) -- ripple off (guarantee the starting mode)
  reaper.Main_OnCommand(40635, 0) -- remove any time selection
  reaper.InsertTrackAtIndex(0, true) -- track is index 0
  local track = reaper.GetTrack(nil, 0)
  local a = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(a, "D_POSITION", 0.0)
  reaper.SetMediaItemInfo_Value(a, "D_LENGTH", 5.0)
  local b = reaper.AddMediaItemToTrack(track)
  reaper.SetMediaItemInfo_Value(b, "D_POSITION", 5.0)
  reaper.SetMediaItemInfo_Value(b, "D_LENGTH", 5.0)
  reaper.SetMediaItemSelected(a, false)
  reaper.SetMediaItemSelected(b, false)
  reaper.SetEditCurPos(2.5, false, false)
  reaper.UpdateArrange()
  -- API-added items have no undo point (see target.lua).
  reaper.Undo_OnStateChange("setup")

  -- Item count of one track by looping GetTrackMediaItem (nil ends the
  -- loop). NOTE: reaper.CountMediaItems(nil, tr) is the PROJECT total, not
  -- the track's count — do not use it here (docs/notes/m5-observations.md
  -- ## M6).
  local function track_items(tr)
    if not tr then return {} end
    local i, items = 0, {}
    while i < 4096 and reaper.GetTrackMediaItem(tr, i) do
      local it = reaper.GetTrackMediaItem(tr, i)
      table.insert(items, {
        p = reaper.GetMediaItemInfo_Value(it, "D_POSITION"),
        l = reaper.GetMediaItemInfo_Value(it, "D_LENGTH"),
      })
      i = i + 1
    end
    return items
  end
  local function count_items()
    local total = 0
    for t = 0, reaper.CountTracks(nil) - 1 do
      total = total + #track_items(reaper.GetTrack(nil, t))
    end
    return total
  end

  check("setup: one track", reaper.CountTracks(nil) == 1,
        "tracks=" .. reaper.CountTracks(nil))
  check("setup: two items", count_items() == 2,
        "items=" .. count_items())
  check("setup: edit cursor at 2.5 s (A is the implicit target)",
        math.abs(reaper.GetCursorPosition() - 2.5) < 1e-6,
        "GetCursorPosition=" .. tostring(reaper.GetCursorPosition()))

  -- Sample every defer cycle for ~1.6 s; the tap lands ~370 ms in, so the
  -- post-gesture state is stable from well before the end.
  local samples = {}
  local t0 = reaper.time_precise()
  wait_ms(1600, function()
    local st = (reaper.time_precise() - t0) * 1000
    table.insert(samples, {
      t = st, tracks = reaper.CountTracks(nil), items = count_items(),
      top = track_items(reaper.GetTrack(nil, 0)),
      src = track_items(reaper.GetTrack(nil, 1)),
    })
  end, function()
    local s
    for i = 1, #samples do
      if samples[i].t >= 600 and samples[i].t <= 1550 then s = samples[i] end
    end
    if not s then
      check("samples collected after the tap", false, "n=" .. #samples)
      finish()
      return
    end

    check("PLACE ON TOP: one new track above the source (1 -> 2)",
          s.tracks == 2, "tracks=" .. s.tracks)
    check("PLACE ON TOP: item count +1 (2 -> 3)", s.items == 3,
          "items=" .. s.items)
    check("PLACE ON TOP: new track (0) holds the copy of A [0,5]",
          #s.top == 1 and math.abs(s.top[1].p) < 1e-6
              and math.abs(s.top[1].l - 5.0) < 1e-6,
          string.format("top=%d items", #s.top))
    check("PLACE ON TOP: source track (1) unchanged (A [0,5], B [5,10])",
          #s.src == 2 and math.abs(s.src[1].p) < 1e-6
              and math.abs(s.src[1].l - 5.0) < 1e-6
              and math.abs(s.src[2].p - 5.0) < 1e-6
              and math.abs(s.src[2].l - 5.0) < 1e-6,
          string.format("src=%d items", #s.src))

    -- Trace: the PLACE ON TOP gesture line, and no `target none` (the tap
    -- must have resolved A as its target).
    local lines = read_trace()
    if lines then
      check("trace: PLACE ON TOP gesture (0005 tap)",
            find_line(lines, " gesture 0005 tap") ~= 0, "")
      local none = 0
      for _, l in ipairs(lines) do
        if l:find("target none", 1, true) then none = none + 1 end
      end
      check("trace: no `target none` (the tap had a target)", none == 0,
            "count=" .. none)
    end

    finish()
  end)
end)
