-- oracle/scenarios/load.lua — does the plugin actually register its actions?
-- One check per action id: NamedCommandLookup("_<id>") ~= 0.
-- Also records reaper.GetAppVersion().
dofile(os.getenv("ORACLE_LIB"))

record("app_version", reaper.GetAppVersion())

local action_ids = {
  "speed_reaper_jogdemo",
  "speed_reaper_scrub_fwd",
  "speed_reaper_scrub_back",
}
for _, id in ipairs(action_ids) do
  local cmd = reaper.NamedCommandLookup("_" .. id)
  check("action " .. id, cmd ~= 0, "NamedCommandLookup(_" .. id .. ")=" .. tostring(cmd))
end

finish()
