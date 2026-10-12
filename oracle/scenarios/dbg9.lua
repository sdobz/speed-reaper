-- oracle/scenarios/dbg9.lua — throwaway: does OnStopButton work headless?
dofile(os.getenv("ORACLE_LIB"))

local function prog(s)
  local f = io.open(os.getenv("ORACLE_OUT") .. "/dbg_progress.txt", "a")
  if f then f:write(reaper.time_precise() .. " " .. s .. "\n") f:close() end
end

run(function()
wait_ms(1500, nil, function() run(function()

prog("warmup; OnPlayButtonEx=" .. type(reaper.OnPlayButtonEx)
     .. " OnStopButton=" .. type(reaper.OnStopButton)
     .. " state=" .. reaper.GetPlayState())
local ok1, err1 = pcall(reaper.OnPlayButtonEx, 0) -- start play
prog("play ok=" .. tostring(ok1) .. " state=" .. reaper.GetPlayState())
check("OnPlayButtonEx returns", ok1, tostring(err1))
wait_ms(300, nil, function()
  local ok2, err2 = pcall(reaper.OnStopButton) -- stop
  prog("stop ok=" .. tostring(ok2) .. " state=" .. reaper.GetPlayState())
  check("OnStopButton returns", ok2, tostring(err2))
  check("state stopped", reaper.GetPlayState() == 0,
        "state=" .. tostring(reaper.GetPlayState()))
  finish()
end)
end) end) end) -- close inner run, warm-up callback, outer run
