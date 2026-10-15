# ReaPack index.xml — intentionally skipped (42-release-prep §6)

ReaPack indexes live on `extensions.reaperforge.com` (per-extension GitHub
repos, `index.xml` listing release zips). Skipped for the 0.1.0 release:

- This repo is not (yet) the ReaPack host repository: it holds the full
  source (speedcli, oracle, tests), and a ReaPack index is only useful once
  there is a stable release cadence + hosting to point at.
- There is no `extensions.reaperforge.com` account/namespace set up for it.

When to add it: create the ReaPack host repo (or add `index.xml` here once
releases are regular), pointing each `<ext>` entry at
`dist/speed-reaper-<version>-<platform>.zip` (the zips already contain the
binary + README + LICENSE + docs/bindings.md + the udev rule, as ReaPack
expects). The release zips themselves (`tools/release`) stay the same.
