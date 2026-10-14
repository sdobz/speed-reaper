-- oracle/run.lua — entry point REAPER is launched with (tools/reaper-oracle).
-- Loads the scenario named by $ORACLE_SCENARIO; a syntax error or a top-level
-- runtime error becomes a failing check + immediate probe.json, instead of a
-- REAPER "ReaScript Error" window and a full scenario timeout.
local lib = os.getenv("ORACLE_LIB")
local path = os.getenv("ORACLE_SCENARIO")
dofile(lib)
local chunk, err = loadfile(path)
if not chunk then
  check("scenario load", false, tostring(err))
  return finish()
end
local ok, rerr = xpcall(chunk, debug.traceback)
if not ok then
  -- the scenario may have re-dofile'd lib; use whatever check/finish are current
  check("scenario error", false, tostring(rerr))
  finish()
end
