-- oracle/scenarios/dbg7.lua — throwaway: progress logging to figure out
-- where the dbg6/spike_rate hang happens (which step stops the defers).
dofile(os.getenv("ORACLE_LIB"))

local function prog(s)
  local f = io.open(os.getenv("ORACLE_OUT") .. "/dbg_progress.txt", "a")
  if f then f:write(reaper.time_precise() .. " " .. s .. "\n") f:close() end
end

run(function()
wait_ms(1500, nil, function() run(function()

prog("warmup done")
prog("type Master_GetPlayRate=" .. type(reaper.Master_GetPlayRate))
local ok, err = pcall(function()
  record("master_rate", tostring(reaper.Master_GetPlayRate(0)))
end)
prog("pcall mrate ok=" .. tostring(ok) .. " err=" .. tostring(err))
check("pcall Master_GetPlayRate", ok, tostring(err))

local cmd = reaper.NamedCommandLookup("_speed_reaper_spike_reset")
prog("reset cmd=" .. tostring(cmd))
local ok2, err2 = pcall(reaper.Main_OnCommand, cmd, 0)
prog("pcall reset ok=" .. tostring(ok2) .. " err=" .. tostring(err2))
check("pcall Main_OnCommand reset", ok2, tostring(err2))

wait_ms(500, nil, function()
  prog("alive after reset, finishing")
  finish()
end)

end) end) end) -- close inner run, warm-up callback, outer run
