-- oracle/scenarios/dbg6.lua — throwaway: why does the reset spike action
-- not fire / why does the spike_rate scenario hang after reverse?
dofile(os.getenv("ORACLE_LIB"))

run(function()
wait_ms(1500, nil, function() run(function()

record("app_version", reaper.GetAppVersion())
check("Master_GetPlayRate is function",
      type(reaper.Master_GetPlayRate) == "function",
      "type=" .. type(reaper.Master_GetPlayRate))
local ok, err = pcall(function()
  record("master_rate", tostring(reaper.Master_GetPlayRate(0)))
end)
check("pcall Master_GetPlayRate", ok, tostring(err))

local cmd = reaper.NamedCommandLookup("_speed_reaper_spike_reset")
check("reset cmd", cmd ~= 0, "cmd=" .. tostring(cmd))
local ok2, err2 = pcall(reaper.Main_OnCommand, cmd, 0)
check("pcall Main_OnCommand reset", ok2, tostring(err2))

wait_ms(800, nil, function()
  check("defer alive after reset", true,
        "playstate=" .. tostring(reaper.GetPlayState()) ..
        " rate=" .. tostring(reaper.Master_GetPlayRate(0)))
  finish()
end)

end) end) end) -- close inner run, warm-up callback, outer run
