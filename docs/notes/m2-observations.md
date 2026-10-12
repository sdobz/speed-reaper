# M2 — hand test observations

## SCRL scroll speed (hand test feedback)

- **JOG**: works as expected — cursor follows the wheel (~20 s of timeline per
  revolution), stops the instant the wheel stops.
- **SCRL (first implementation)**: too sensitive. It used
  `CSurf_OnScroll(±1)` per 20 wheel units (`kScrollUnitsPerStep`), i.e.
  ~73,000 line-steps per wheel revolution — the view flew across the screen,
  nothing like the cursor's travel.
- **Fix**: SCRL now scrolls the arrange view in **timeline seconds** —
  `GetSet_ArrangeView2` reads the view start/end times, shifts both by
  `delta × JogEngine::kSecondsPerWheelUnit`, writes them back, then
  `UpdateArrange()`. Same scale as the jog cursor (20 s/revolution), so the
  view travels the same on-screen distance per wheel unit as the cursor —
  at any zoom (seconds already account for zoom; no zoom read needed).
  `IActionSink::ScrollView(int)` became `ScrollBySeconds(double)`; the
  router's remainder accumulation is gone.
- Oracle `buttons` scenario gained two trace checks: `scroll dt=` lines are
  present, and none appears before the first `mode Scrl`.
- **Status**: awaiting re-test of SMOKE-TEST.md step 2 (and the M2 pass bar).
