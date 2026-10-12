#!/usr/bin/env python3
"""Generate wheel_turn.txt (vibes/19-oracle-device-windows.md task 3):
`at 300 wheel 20` repeated every 8 ms for 500 ms, then 20 deltas decaying
to 0 over the next 500 ms. Usage: gen_wheel_turn.py > wheel_turn.txt"""

lines = []
# Phase 1: steady `wheel 20` every 8 ms, starting at t=300, for 500 ms.
for i in range(500 // 8):
    lines.append(f"at {300 + 8 * i} wheel 20")
# Phase 2: 20 deltas decaying 20 -> 1 (i.e. toward 0), 25 ms apart = 500 ms.
t0 = 300 + 500
for i in range(20):
    lines.append(f"at {t0 + 25 * i} wheel {20 - i}")
print("\n".join(lines))
