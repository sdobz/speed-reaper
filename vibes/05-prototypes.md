The next step is to build demos

# Speed Editor hello world

- [x] Locate a speed editor reference (python? c++?) and store in `docs/speed-editor-examples`
  Done: both languages exist and are vendored in `docs/speed-editor-examples/` — `python-bmd/` (octimot/blackmagic-speededitor @ 0919407, the original protocol ref: auth, reports, jog modes, 34 keys), `python-driver/` + `cpp-driver/` (John-Howard/PythonSpeedEditor @ 28057b4, BSD-3: self-contained Python driver with verified keycodes + C++17 hidapi driver with the challenge-response auth ported; `hid_device.cpp` verified to pass `-fsyntax-only` against dev-shell hidapi with a 3-line flog stub). `README.md` in that dir has the provenance table + protocol cheat sheet + notes for tasks 3–4.
- [ ] Describe the capabilities of the speed editor including all buttons in `docs/speed-editor-capabilities.md`
- [ ] Build as plain as possible cli in the nix dev env connecting to the editor and demonstrating control capabilities. Write instructions for how to test in SMOKE-TEST.md
- [ ] Build a hello-world davinci plugin to demonstrate that we can achieve smooth playhead control. Write instructions for how to test in SMOKE-TEST.md

