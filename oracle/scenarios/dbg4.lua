-- debug: defer only, no setup, no action
dofile(os.getenv("ORACLE_LIB"))
local function f() record("defer_ran", "yes") finish() end
reaper.defer(f)
