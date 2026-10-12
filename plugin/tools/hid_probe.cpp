// hid_probe — standalone Speed Editor probe tool
// (docs/plugin-design-m1-m3.md §5). Links HidWheelSource + driver, no REAPER.
//
// Usage: hid_probe [--seconds N] [--buttons]
//   Starts the wheel source, then every 8 ms calls Tick/PopLog/(PopButton),
//   printing log lines as they arrive and the running wheel sum once per
//   second. Exits 0 after N seconds (default 5) whether or not a device was
//   found. Output prefixes: `log: `, `wheel: sum=<n>`,
//   `button: <key hex> down|up` (buttons only with --buttons).

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "hid_wheel_source.h"

int main(int argc, char **argv) {
  int seconds = 5;
  bool want_buttons = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--seconds" && i + 1 < argc) {
      seconds = std::atoi(argv[++i]);
    } else if (a == "--buttons") {
      want_buttons = true;
    } else {
      std::fprintf(stderr, "unknown arg: %s (usage: hid_probe [--seconds N] "
                           "[--buttons])\n",
                   a.c_str());
      return 2;
    }
  }
  if (seconds <= 0) {
    std::fprintf(stderr, "--seconds must be > 0\n");
    return 2;
  }

  HidWheelSource src;
  src.Start();
  const auto tick_start = std::chrono::steady_clock::now();
  std::int64_t sum = 0;
  int next_print_s = 1;
  int elapsed_s = 0;
  while (elapsed_s < seconds) {
    WheelSample sample;
    src.Tick(&sample);
    sum += sample.delta;
    std::string line;
    while (src.PopLog(&line)) {
      std::printf("log: %s\n", line.c_str());
    }
    if (want_buttons) {
      ButtonChange btn;
      while (src.PopButton(&btn)) {
        std::printf("button: %04x %s\n", btn.key,
                    btn.down ? "down" : "up");
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(8));
    elapsed_s = (int)std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - tick_start)
                    .count();
    if (elapsed_s >= next_print_s) {
      std::printf("wheel: sum=%lld\n", (long long)sum);
      std::fflush(stdout);
      next_print_s += 1;
    }
  }
  src.Stop();
  return 0;
}
