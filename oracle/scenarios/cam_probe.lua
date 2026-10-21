-- Documented probe: intentionally NOT in the tools/reaper-oracle REAPER_SCENARIOS suite (42-release-prep §5). Keep for manual runs: --scenario NAME.
-- oracle/scenarios/cam_probe.lua — CAM N research (vibes/45 task 1).
-- No fake device. Layout: two tracks, one item each spanning [0,2] so both
-- overlap the edit cursor (1.0). Probes:
--   (a) B_UISEL set/clear on the track-2 item leaves the track-1 item's
--       B_UISEL and B_SELECTED untouched (per-track selection independence).
--   (b) the edit cursor does not move on a B_UISEL change.
--   (c) native "select/toggle item" candidates from docs/reaper-actions.tsv:
--       40528 select item under mouse cursor (exclusive)
--       40529 select item under mouse cursor (leaving other items selected)
--       40530 toggle selection of item under mouse cursor
--       40416 item navigation: select and move to previous item
--       40417 item navigation: select and move to next item
--       For each: before/after state of both items (B_UISEL, B_SELECTED) and
--       the edit cursor, run twice to classify toggle vs exclusive and to see
--       whether the action does anything headless at all.
-- See docs/notes/cam-n-select.md (Decisions + Holes H1/H2).
dofile(os.getenv("ORACLE_LIB"))

local items = {}   -- [1] = track 1 item, [2] = track 2 item
local CURSOR = 1.0

local function later(fn)
  reaper.defer(function()
    local ok, err = pcall(fn)
    if not ok then
      record("chain_error", tostring(err))
      check("probe done", true, "")
      finish()
    end
  end)
end

-- "ui sel" flags for both items plus edit cursor
local function snap(tag)
  local parts = {}
  for i = 1, 2 do
    table.insert(parts, "t" .. i .. " ui=" ..
      tostring(reaper.GetMediaItemInfo_Value(items[i], "B_UISEL")) ..
      " sel=" ..
      tostring(reaper.GetMediaItemInfo_Value(items[i], "B_SELECTED")))
  end
  table.insert(parts, "cursor=" .. tostring(reaper.GetCursorPosition()))
  local s = table.concat(parts, " | ")
  record(tag, s)
  return s
end

local function expect(label, cond, detail)
  check(label, cond, label .. ": " .. detail)
end

local candidates = {
  { id = 40528, name = "select item under mouse cursor (exclusive)" },
  { id = 40529, name = "select item under mouse cursor (leaving other selected)" },
  { id = 40530, name = "toggle selection of item under mouse cursor" },
  { id = 40416, name = "select and move to previous item" },
  { id = 40417, name = "select and move to next item" },
}

local function probe_candidates(i)
  if i > #candidates then
    finish()
    return
  end
  local c = candidates[i]
  -- reset to a known state so probes are independent
  reaper.SetMediaItemInfo_Value(items[1], "B_UISEL", 0)
  reaper.SetMediaItemInfo_Value(items[1], "B_SELECTED", 0)
  reaper.SetMediaItemInfo_Value(items[2], "B_UISEL", 0)
  reaper.SetMediaItemInfo_Value(items[2], "B_SELECTED", 0)
  reaper.SetEditCurPos(CURSOR, false, false)
  reaper.UpdateArrange()
  later(function()
    local s0 = snap("cand" .. c.id .. "_0")
    reaper.Main_OnCommand(c.id, 0)
    reaper.UpdateArrange()
    later(function()
      local s1 = snap("cand" .. c.id .. "_1")
      reaper.Main_OnCommand(c.id, 0)
      reaper.UpdateArrange()
      later(function()
        local s2 = snap("cand" .. c.id .. "_2")
        local works = (s1 ~= s0)
        local reverts = (s2 == s0)
        expect("candidate " .. c.id .. " " .. c.name .. " (recorded)",
               true,
               "before=[" .. s0 .. "] after1=[" .. s1 .. "] after2=[" ..
               s2 .. "] works_headless=" .. tostring(works) ..
               " reverts_on_2nd_run=" .. tostring(reverts) ..
               (works and (reverts and " => toggle" or " => sticky/exclusive") or " => no-op headless"))
        probe_candidates(i + 1)
      end)
    end)
  end)
end

start("settled", function()
  record("app_version", reaper.GetAppVersion())
  local n = reaper.CountTracks(nil)
  for _ = 1, 2 - n do reaper.InsertTrackAtIndex(n, true) end
  for t = 0, 1 do
    local tr = reaper.GetTrack(nil, t)
    local it = reaper.AddMediaItemToTrack(tr)
    reaper.AddTakeToMediaItem(it, true)
    reaper.SetMediaItemInfo_Value(it, "D_POSITION", 0.0)
    reaper.SetMediaItemInfo_Value(it, "D_LENGTH", 2.0)
    items[t + 1] = it
  end
  reaper.SetEditCurPos(CURSOR, false, false)
  reaper.UpdateArrange()
  later(function()
    check("setup: two items overlap the cursor",
          reaper.GetMediaItemInfo_Value(items[1], "D_POSITION") == 0.0 and
          reaper.GetMediaItemInfo_Value(items[2], "D_POSITION") == 0.0 and
          reaper.GetCursorPosition() == CURSOR,
          snap("baseline"))

    -- (a) + (b): B_UISEL set on the track-2 item
    local t1u = reaper.GetMediaItemInfo_Value(items[1], "B_UISEL")
    local t1s = reaper.GetMediaItemInfo_Value(items[1], "B_SELECTED")
    local cursor_before = reaper.GetCursorPosition()
    reaper.SetMediaItemInfo_Value(items[2], "B_UISEL", 1)
    reaper.UpdateArrange()
    later(function()
      local t2u = reaper.GetMediaItemInfo_Value(items[2], "B_UISEL")
      local t1u2 = reaper.GetMediaItemInfo_Value(items[1], "B_UISEL")
      local t1s2 = reaper.GetMediaItemInfo_Value(items[1], "B_SELECTED")
      local cursor_after = reaper.GetCursorPosition()
      expect("a1: B_UISEL set on track-2 item works", t2u == 1, snap("a1"))
      expect("a2: track-1 item selection untouched by track-2 B_UISEL set",
             t1u2 == t1u and t1s2 == t1s, snap("a2"))
      expect("b1: edit cursor unchanged by B_UISEL set",
             cursor_after == cursor_before,
             "before=" .. tostring(cursor_before) ..
             " after=" .. tostring(cursor_after))

      -- (a) + (b): B_UISEL cleared on the track-2 item
      reaper.SetMediaItemInfo_Value(items[2], "B_UISEL", 0)
      reaper.UpdateArrange()
      later(function()
        local t2u2 = reaper.GetMediaItemInfo_Value(items[2], "B_UISEL")
        local t1u3 = reaper.GetMediaItemInfo_Value(items[1], "B_UISEL")
        local t1s3 = reaper.GetMediaItemInfo_Value(items[1], "B_SELECTED")
        local cursor_after2 = reaper.GetCursorPosition()
        expect("a3: B_UISEL clear on track-2 item works", t2u2 == 0, snap("a3"))
        expect("a4: track-1 item selection untouched by track-2 B_UISEL clear",
               t1u3 == t1u and t1s3 == t1s, snap("a4"))
        expect("b2: edit cursor unchanged by B_UISEL clear",
               cursor_after2 == cursor_before,
               "before=" .. tostring(cursor_before) ..
               " after=" .. tostring(cursor_after2))

        -- (c): native candidates, two runs each
        probe_candidates(1)
      end)
    end)
  end)
end)
