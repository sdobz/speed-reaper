-- oracle/scenarios/spike_rate.lua — M4 playrate spike under the oracle.
--
-- For each spike action (rate 2x, 8x, reverse -1x, reset) run it via
-- Main_OnCommand(NamedCommandLookup(...), 0), wait 600 ms, then record in
-- the check details: the plugin's `spike ...` trace values,
-- reaper.Master_GetPlayRate(0), reaper.GetPlayState(), and two cursor
-- samples 300 ms apart (edit cursor via GetCursorPosition, plus the
-- playhead via GetPlayPositionEx — the edit cursor does NOT follow the
-- playhead during playback, so the playhead samples are the ones that
-- show motion). The checks only assert that each action ran (its trace
-- line is present); the measurements are the output.
--
-- Sequential by construction: wait_ms is non-blocking (it defers), so the
-- actions are chained through the 600 ms wait's on_done callback — one
-- action in flight at a time. The plugin prints its own ~300 ms
-- measurement into the trace, which must land inside the 600 ms window.
dofile(os.getenv("ORACLE_LIB"))

local actions = {
  { name = "rate2",   cmd = "_speed_reaper_spike_rate2",   rate = "2.000000" },
  { name = "rate8",   cmd = "_speed_reaper_spike_rate8",   rate = "8.000000" },
  { name = "reverse", cmd = "_speed_reaper_spike_reverse", rate = "-1.000000" },
  { name = "reset",   cmd = "_speed_reaper_spike_reset",   rate = "1.000000" },
}

local function read_trace()
  local f = io.open(os.getenv("ORACLE_OUT") .. "/trace_spike_rate.txt", "r")
  if not f then return {} end
  local lines = {}
  for line in f:lines() do table.insert(lines, line) end
  f:close()
  return lines
end

local function last_matching(lines, needle)
  for i = #lines, 1, -1 do
    if lines[i]:find(needle, 1, true) then return lines[i] end
  end
  return nil
end

start("settled", function()

record("app_version", reaper.GetAppVersion())

-- Look up and sanity-check all four actions first.
local cmds = {}
for _, a in ipairs(actions) do
  local cmd = reaper.NamedCommandLookup(a.cmd)
  cmds[a.name] = cmd
  check("action " .. a.cmd, cmd ~= 0,
        "NamedCommandLookup(" .. a.cmd .. ")=" .. tostring(cmd))
end

local i = 0
local function next_action()
  i = i + 1
  if i > #actions then finish() return end
  local a = actions[i]

  -- Known start point so "cursor moved?" is meaningful.
  reaper.SetEditCurPos(10, false, false)

  local samples = {} -- {t, pos = edit cursor, play = playhead}
  local t0 = reaper.time_precise()
  reaper.Main_OnCommand(cmds[a.name], 0)

  wait_ms(600, function()
    table.insert(samples, { t = reaper.time_precise() - t0,
                            pos = reaper.GetCursorPosition(),
                            play = reaper.GetPlayPositionEx(0) })
  end, function()
    -- Samples 300 ms apart: first sample at/after t=0.3 s and the last
    -- (t~0.6 s). Motion is judged on the playhead (the edit cursor is
    -- stationary during playback).
    local pa, pb, qa, qb = nil, nil, nil, nil
    for j = 1, #samples do
      if pa == nil and samples[j].t >= 0.28 then
        pa, qa = samples[j].pos, samples[j].play
      end
      pb, qb = samples[j].pos, samples[j].play
    end
    if pa == nil and #samples > 0 then pa, qa = samples[1].pos, samples[1].play end

    -- The plugin's own ~300 ms measurement (spike set/rate/reverse lines).
    local lines = read_trace()
    local set_needle = a.name == "reset"
        and "spike set rate=1.0 (reset, stop)"
        or ("spike set rate=" .. a.rate)
    local setline = last_matching(lines, set_needle)
    local rateline = last_matching(lines, "spike rate=" .. a.rate)
    local revline = a.name == "reverse"
        and last_matching(lines, "spike reverse pos0=") or nil

    local read_rate = reaper.Master_GetPlayRate(0)
    local state = reaper.GetPlayState()
    local moved = qa ~= nil and qb ~= nil and (qb - qa) > 1e-3
    local reversed = qa ~= nil and qb ~= nil and (qb - qa) < -1e-3

    check(a.name .. " ran (trace line present)", rateline ~= nil,
          string.format("requested=%s | trace: %s | set: %s | reverse: %s | "
                        .. "readback=%.4f playstate=%d cursor_a=%.4f "
                        .. "cursor_b=%.4f playhead_a(%.0fms)=%.4f "
                        .. "playhead_b(%.0fms)=%.4f moved=%s reversed=%s (n=%d)",
                        a.rate,
                        rateline or "<none>",
                        setline or "<none>",
                        revline or "<n/a>",
                        read_rate, state,
                        pa or 0, pb or 0,
                        qa ~= nil and 300 or 0, qa or 0,
                        qb ~= nil and 600 or 0, qb or 0,
                        tostring(moved), tostring(reversed), #samples))
    next_action()
  end)
end
next_action()

end)
