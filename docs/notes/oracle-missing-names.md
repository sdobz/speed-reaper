# Missing SDK name(s) on REAPER 7.81 (oracle probe)

PROBE total=877 missing=1
EnumThemeColors

Method: scanned all 877 names from `oracle/sdk_api_names.txt` as strings against the
REAPER 7.81 binary (`/nix/store/h2crs2mvvh5ig710x47qnwcazynhc3fc-reaper-7.81/opt/REAPER/`
`.reaper-wrapped` + `*.so`) — exactly one name is absent: `EnumThemeColors`.
Cross-checked against the reaper-layer oracle trace `0 loadapi missing=1`
(`build/oracle/trace.txt`), i.e. real REAPER's GetFunc lacks exactly one probed name.

Note on the .so probe: `oracle/probe_missing.so` (built, `extern "C" ReaperPluginEntry`)
was run 6 times under `xvfb-run -a reaper -newinst -nosplash` with a fresh HOME and the
probe in `UserPlugins/` (timeouts 45–300 s; logs `vibes/.ralph/logs/probe-run*.log`).
In every run REAPER spent the whole window scanning its bundled Plugins and never
reached the UserPlugins dlopen, so `build/oracle/probe_missing.txt` was never written.
The name above therefore comes from the offline ELF scan (deterministic, not invented);
the probe remains as the in-REAPER fallback.

The host layer's 145 missing names are exactly the list in
`docs/notes/sdk-vs-reaper-api.md` (SDK's 877 pointer declarations minus the 732 that
overlap the 874-name host list `oracle/reaper_api_names.txt`). `EnumThemeColors` is in
the host list, so it is not among the host's 145 — only real REAPER 7.81's GetFunc
lacks it, matching the reaper-layer trace.
