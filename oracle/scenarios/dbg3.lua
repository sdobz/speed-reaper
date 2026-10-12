-- debug: cursor setup only, no action, no defer
dofile(os.getenv("ORACLE_LIB"))
reaper.SetEditCurPos(10, false, false)
reaper.GetSetTransportInfo(0, "RATE", 0)
record("pos", reaper.GetCursorPosition())
finish()
