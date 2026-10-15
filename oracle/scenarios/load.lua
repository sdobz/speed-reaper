-- oracle/scenarios/load.lua — does the plugin actually load and initialize?
-- The plugin registers no actions, so prove the entry point ran to the end
-- from its trace file ($SPEED_REAPER_TRACE): it must have registered its
-- ext fields, the ~125 Hz timer, and the HID worker before returning 1.
-- Also records reaper.GetAppVersion().
dofile(os.getenv("ORACLE_LIB"))

record("app_version", reaper.GetAppVersion())

local lines = read_trace()
for _, needle in ipairs({ "entry result=1", "register timer ok", "hid source started" }) do
  local i = lines and find_line(lines, needle) or 0
  check(needle, i ~= 0)
end

finish()
