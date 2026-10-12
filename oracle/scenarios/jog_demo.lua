-- oracle/scenarios/jog_demo.lua — behavior check for the jog demo (design §5).
--
-- Setup: empty project, edit cursor at 10 s, transport stopped.
-- Run the "Speed Reaper: jog demo" action, sample GetCursorPosition() every
-- defer cycle for 3 s, then check:
--   1. final - start in [0.3, 0.8] s (design ≈ 0.5 s)
--   2. cursor never decreases
--   3. max step between consecutive samples < 0.05 s ("no stutter")
--   4. last 0.5 s of samples are constant (coasts to a stop)
dofile(os.getenv("ORACLE_LIB"))

run(function()
-- Warm-up: REAPER shows startup dialogs in a fresh profile (dismissed by
-- tools/reaper-oracle); wait for the UI to settle so their stall isn't
-- mistaken for jog stutter.
wait_ms(1500, nil, function() run(function()

record("app_version", reaper.GetAppVersion())

local cmd = reaper.NamedCommandLookup("_speed_reaper_jogdemo")
check("action speed_reaper_jogdemo", cmd ~= 0,
      "NamedCommandLookup(_speed_reaper_jogdemo)=" .. tostring(cmd))

reaper.SetEditCurPos(10, false, false)
check("transport stopped", reaper.GetPlayState() == 0, "GetPlayState=" .. tostring(reaper.GetPlayState()))

local start = reaper.GetCursorPosition()
reaper.Main_OnCommand(cmd, 0)

local samples = {} -- {t = s since trigger, pos = cursor position}
local t0 = reaper.time_precise()
local function sample()
  table.insert(samples, { t = reaper.time_precise() - t0,
                          pos = reaper.GetCursorPosition() })
end

wait_ms(3000, sample, function()
  local n = #samples
  local final = n > 0 and samples[n].pos or start
  local disp = final - start
  check("displacement in [0.3,0.8] s", disp >= 0.3 and disp <= 0.8,
        string.format("start=%.5f final=%.5f disp=%.5f (n=%d samples, %.0f/s sampling)",
                      start, final, disp, n,
                      n > 1 and n / samples[n].t or 0))

  local mono = true
  for i = 2, n do
    if samples[i].pos < samples[i - 1].pos - 1e-9 then mono = false break end
  end
  check("cursor never decreases", mono,
        string.format("n=%d min=%.3f final=%.3f", n, start, final))

  local maxstep = 0
  for i = 2, n do
    local s = samples[i].pos - samples[i - 1].pos
    if s > maxstep then maxstep = s end
  end
  check("max step < 0.05 s", maxstep < 0.05,
        string.format("maxstep=%.4f (n=%d)", maxstep, n))

  local last_t = n > 0 and samples[n].t or 0
  local tail, m = {}, 0
  for i = 1, n do
    if last_t - samples[i].t <= 0.5 then m = m + 1 tail[m] = samples[i].pos end
  end
  local const = m > 0
  if const then
    for i = 2, m do
      if math.abs(tail[i] - tail[1]) > 1e-6 then const = false break end
    end
  end
  check("last 0.5 s constant (coast to stop)", const,
        string.format("tail=%d samples first=%.3f last=%.3f",
                      m, tail[1] or -1, tail[m] or -1))

  finish()
end)
end) end) end) -- close inner run, warm-up callback, outer run
