-- Oracle scenario: one trivial check, then finish. For crash-capture testing
-- (vibes/18-oracle-linux.md task 6) combined with SPEED_REAPER_TEST_CRASH,
-- and as a startup/teardown timing baseline.
-- (check/finish come from oracle/lib.lua, loaded by oracle/run.lua.)
check("finished", true)
finish()
