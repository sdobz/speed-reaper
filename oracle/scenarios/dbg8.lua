-- oracle/scenarios/dbg8.lua — throwaway: which nested command hangs?
-- 40045 (stop) and 1007 (play) straight from Lua, with progress logging.
dofile(os.getenv("ORACLE_LIB"))

local function prog(s)
  local f = io.open(os.getenv("ORACLE_OUT") .. "/dbg_progress.txt", "a")
  if f then f:write(reaper.time_precise() .. " " .. s .. "\n") f:close() end
end

run(function()
wait_ms(1500, nil, function() run(function()

prog("warmup done; state=" .. reaper.GetPlayState())
local ok1, err1 = pcall(reaper.Main_OnCommand, 40045, 0) -- stop (already stopped)
prog("stop(stopped) ok=" .. tostring(ok1) .. " err=" .. tostring(err1))
check("stop while stopped returns", ok1, tostring(err1))

local ok2, err2 = pcall(reaper.Main_OnCommand, 1007, 0) -- play
prog("play ok=" .. tostring(ok2) .. " err=" .. tostring(err2))
check("play returns", ok2, tostring(err2))

local ok3, err3 = pcall(reaper.Main_OnCommand, 40045, 0) -- stop (while playing)
prog("stop(playing) ok=" .. tostring(ok3) .. " err=" .. tostring(err3))
check("stop while playing returns", ok3, tostring(err3))

finish()
end) end) end) -- close inner run, warm-up callback, outer run
