// speedcli — plain-CLI smoke test for the Blackmagic Speed Editor.
// Task: vibes/05-prototypes.md #3 ("build as plain as possible cli in the
// nix dev env connecting to the editor and demonstrating control
// capabilities"). Manual test steps: SMOKE-TEST.md.
//
// Commands:
//   scan    list Speed Editors on the USB bus (exit 1 if none)
//   listen  authenticate, decode every input report, and drive the
//           device's own outputs from its buttons (default command)
//   demo    authenticate, then walk all 21 LEDs so each can be verified
//   mode N  authenticate, set wheel protocol mode N (0-3) + jog LED, exit
//
// Options:
//   -v    hexdump every raw input report
//   -h    help
//
// Exit codes: 0 ok, 1 no device / open failed, 2 auth failed,
//             3 device write failed, 64 usage error.
//
// The protocol implemented here is documented in
// docs/speed-editor-capabilities.md; the driver in src/speed_editor is a
// port of the vendored C++ reference (docs/speed-editor-examples/cpp-driver).

#include "speed_editor/hid_device.h"
#include <hidapi/hidapi.h>  // cmdScan lists devices directly (not opened)
#include "speed_editor/key_map.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <functional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

bool g_verbose = false;
volatile sig_atomic_t g_quit = 0;

void onSignal(int) { g_quit = 1; }

void hexdump(const std::vector<uint8_t>& raw) {
    if (!g_verbose) { return; }
    for (uint8_t b : raw) { std::printf("%02X ", b); }
    std::printf("\n");
}

const char* wheelModeName(uint8_t mode) {
    switch (mode) {
        case 0: return "RELATIVE_0 (jog)";
        case 1: return "ABS_CONT (shuttle)";
        case 2: return "RELATIVE_2 (scroll)";
        case 3: return "ABS_DEADZERO";
        default: return "unknown";
    }
}

const char* keyName(uint16_t kc) {
    const auto& names = getButtonNames();
    auto it = names.find(kc);
    return it == names.end() ? nullptr : it->second.c_str();
}

std::string keyHex(uint16_t kc) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "0x%04X", kc);
    return buf;
}

// The 18 main-panel LEDs in bit order (bit positions from key_map.h —
// note the CAM bits are not in camera order; do not "fix" that).
struct LedInfo { int bit; const char* name; };
const LedInfo MAIN_LEDS[] = {
    {LED_CLOSE_UP, "CLOSE_UP"}, {LED_CUT, "CUT"}, {LED_DIS, "DIS"},
    {LED_SMTH_CUT, "SMTH_CUT"}, {LED_TRANS, "TRANS"}, {LED_SNAP, "SNAP"},
    {LED_CAM7, "CAM7"}, {LED_CAM8, "CAM8"}, {LED_CAM9, "CAM9"},
    {LED_LIVE_OWR, "LIVE_OWR"}, {LED_CAM4, "CAM4"}, {LED_CAM5, "CAM5"},
    {LED_CAM6, "CAM6"}, {LED_VIDEO_ONLY, "VIDEO_ONLY"}, {LED_CAM1, "CAM1"},
    {LED_CAM2, "CAM2"}, {LED_CAM3, "CAM3"}, {LED_AUDIO_ONLY, "AUDIO_ONLY"},
};

// ── shared: enumerate / open / auth ────────────────────────────────

std::string wideToUtf8(const wchar_t* w) {
    std::string s;
    if (w) { for (const wchar_t* p = w; *p; p++) { s += (char)*p; } }
    return s;
}

int cmdScan() {
    struct hid_device_info* devs = hid_enumerate(SPEED_EDITOR_VID, SPEED_EDITOR_PID);
    // NOTE: the hidraw backend also returns NULL when the machine has no
    // /dev/hidraw* nodes at all, so NULL == "not found", not a hard error.
    int count = 0;
    for (struct hid_device_info* d = devs; d; d = d->next) {
        count++;
        std::printf("Speed Editor found:\n"
                    "  path   : %s\n"
                    "  serial : %s\n",
                    d->path, wideToUtf8(d->serial_number).c_str());
    }
    if (devs) { hid_free_enumeration(devs); }
    if (count == 0) {
        std::printf("No Speed Editor (VID 0x%04X PID 0x%04X) on the USB bus.\n",
                    SPEED_EDITOR_VID, SPEED_EDITOR_PID);
        std::printf("Check: ls /dev/hidraw*   (empty means the hidraw driver\n"
                    "       has no HID device — plug in the editor over USB-C)\n");
        return 1;
    }
    return 0;
}

// Returns 0 on success, or a process exit code.
int connectEditor(SpeedEditorHID& ed, const char* purpose) {
    if (!ed.open()) {
        std::fprintf(stderr,
            "error: could not open the Speed Editor (%s).\n"
            "  - is it plugged in?          check: ./build/speedcli scan\n"
            "  - is DaVinci Resolve or another app holding it? Close it —\n"
            "    only one host can claim the device at a time.\n"
            "  - do you have /dev/hidraw access?  see SMOKE-TEST.md (udev rule)\n",
            purpose);
        return 1;
    }
    std::printf("device : Speed Editor (serial %s)\n",
                ed.getSerial().empty() ? "none" : ed.getSerial().c_str());
    const int reauth_sec = ed.authenticate();
    if (reauth_sec <= 0) {
        std::fprintf(stderr, "error: authentication failed (challenge/response on report 0x06)\n");
        ed.close();
        return 2;
    }
    std::printf("auth   : OK (%s, device re-locks in ~%ds)\n", purpose, reauth_sec);
    return 0;
}

// ── listen: the default command ────────────────────────────────────

// Handle a button press that demonstrates *output* control from input.
void controlFromButton(SpeedEditorHID& ed, uint16_t kc, uint32_t& mainLeds,
                       std::function<bool()> sendMainLeds) {
    switch (kc) {
        // The three mode buttons reconfigure the wheel and light their LED.
        case KEY_SHTL:
            if (ed.setWheelMode(1) && ed.setJogLeds(false, true, false)) {
                std::printf("control : wheel mode 1 (shuttle/absolute) + SHTL LED on\n");
            }
            break;
        case KEY_JOG:
            if (ed.setWheelMode(0) && ed.setJogLeds(true, false, false)) {
                std::printf("control : wheel mode 0 (jog/relative) + JOG LED on\n");
            }
            break;
        case KEY_SCRL:
            if (ed.setWheelMode(2) && ed.setJogLeds(false, false, true)) {
                std::printf("control : wheel mode 2 (scroll/relative) + SCRL LED on\n");
            }
            break;
        // SNAP toggles one of the 18 main-panel LEDs (report 0x02).
        case KEY_SNAP:
            mainLeds ^= (1u << LED_SNAP);
            if (sendMainLeds()) {
                std::printf("control : SNAP LED %s\n",
                            (mainLeds >> LED_SNAP) & 1 ? "on" : "off");
            }
            break;
        default:
            break;
    }
    if (kc == KEY_STOP_PLAY) {
        std::printf("note    : STOP/PLAY pressed (the CLI has no transport target)\n");
    }
}

int cmdListen() {
    SpeedEditorHID ed;
    int rc = connectEditor(ed, "listen");
    if (rc != 0) { return rc; }

    // Initialise the wheel to relative/jog and light the matching mode LED.
    if (!ed.setWheelMode(0) || !ed.setJogLeds(true, false, false)) {
        std::fprintf(stderr, "error: failed to send initial wheel mode report\n");
        ed.close();
        return 3;
    }
    std::printf("wheel  : mode 0 (jog/relative) set, JOG LED on\n");
    std::printf("try    : spin the wheel, press SHTL/JOG/SCRL (wheel mode + LED),\n"
                "         SNAP (toggles its LED), any other button, unplug USB\n"
                "         (battery report). ESC or Ctrl-C exits.\n\n");

    std::set<uint16_t> held;      // keycodes currently down
    uint32_t mainLeds = 0;        // main-panel LED state we own (bit index = LED_*)

    auto sendMainLeds = [&]() {
        const auto b = LedsReportBytes(mainLeds);
        return ed.setLeds(b[0], b[1], b[2]);
    };

    SpeedEditorEvent ev;
    while (!g_quit) {
        if (!ed.readEvent(ev, 100)) {
            if (ed.getState() == SpeedEditorHID::State::Disconnected) {
                std::printf("\ndevice : disconnected\n");
                break;
            }
            continue;  // read timeout, nothing happened
        }
        hexdump(ev.raw);
        switch (ev.type) {
            case EventType::Wheel:
                std::printf("wheel  : delta=%+d  mode=%u %s\n",
                            ev.wheel.delta, (unsigned)ev.wheel.mode,
                            wheelModeName(ev.wheel.mode));
                break;
            case EventType::Button: {
                std::set<uint16_t> now(ev.button.keycodes.begin(),
                                       ev.button.keycodes.end());
                if (now.count(KEY_ESC)) {
                    std::printf("\nbutton : ESC — exiting\n");
                    g_quit = 1;
                    break;
                }
                for (uint16_t kc : now) {
                    if (held.count(kc)) { continue; }
                    held.insert(kc);
                    const char* nm = keyName(kc);
                    std::printf("button : down  %-13s (%s)\n",
                                nm ? nm : "?", keyHex(kc).c_str());
                    controlFromButton(ed, kc, mainLeds, sendMainLeds);
                }
                std::set<uint16_t> toRelease;
                for (uint16_t kc : held) {
                    if (!now.count(kc)) { toRelease.insert(kc); }
                }
                for (uint16_t kc : toRelease) {
                    held.erase(kc);
                    const char* nm = keyName(kc);
                    std::printf("button : up    %s\n", nm ? nm : keyHex(kc).c_str());
                }
                break;
            }
            case EventType::Battery:
                std::printf("battery: charging=%d level=%d%%\n",
                            (int)ev.battery.charging, ev.battery.level);
                break;
            case EventType::Unknown:
                std::printf("report : unknown input report\n");
                break;
        }
    }

    ed.close();  // also clears all LEDs
    std::printf("bye    : closed, all LEDs off\n");
    return 0;
}

// ── demo: LED chase over all 21 LEDs ───────────────────────────────

int cmdDemo() {
    SpeedEditorHID ed;
    int rc = connectEditor(ed, "LED demo");
    if (rc != 0) { return rc; }

    std::printf("chase  : main panel LEDs (report 0x02) — watch the panel:\n");
    for (const LedInfo& led : MAIN_LEDS) {
        const auto b = LedsReportBytes(1u << led.bit);
        ed.setLeds(b[0], b[1], b[2]);
        ed.setJogLeds(false, false, false);
        std::printf("  bit %2d  %s\n", led.bit, led.name);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    ed.setLeds(0x00, 0x00, 0x00);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    std::printf("chase  : jog mode LEDs (report 0x04):\n");
    const char* jogNames[] = {"JOG", "SHTL", "SCRL"};
    for (int i = 0; i < 3; i++) {
        ed.setJogLeds(i == 0, i == 1, i == 2);
        std::printf("  bit %d  %s\n", i, jogNames[i]);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    ed.setJogLeds(false, false, false);

    std::printf("chase  : all 21 LEDs on, then off:\n");
    ed.setLeds(0xFF, 0xFF, 0x03);  // byte 2 only has 2 defined bits (CAM3, AUDIO_ONLY)
    ed.setJogLeds(true, true, true);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    ed.close();  // also clears all LEDs
    std::printf("demo   : complete — all LEDs off\n");
    return 0;
}

// ── mode: set the wheel protocol mode and exit ─────────────────────

int cmdMode(const char* arg) {
    if (!arg) {
        std::fprintf(stderr, "usage: speedcli mode <0|1|2|3>\n");
        return 64;
    }
    int m = std::atoi(arg);
    if (m < 0 || m > 3) {
        std::fprintf(stderr, "mode must be 0-3 (got \"%s\")\n", arg);
        return 64;
    }
    SpeedEditorHID ed;
    int rc = connectEditor(ed, "set mode");
    if (rc != 0) { return rc; }

    if (!ed.setWheelMode((uint8_t)m)) {
        std::fprintf(stderr, "error: failed to send wheel mode report\n");
        ed.close();
        return 3;
    }
    // Light the user-mode button LED for modes 0-2; mode 3 has no button.
    ed.setJogLeds(m == 0, m == 1, m == 2);
    std::printf("wheel  : mode %d %s set, jog LEDs %s\n", m,
                wheelModeName((uint8_t)m), m == 3 ? "off (no button)" : "updated");
    ed.close();
    return 0;
}

int usage() {
    std::printf(
        "speedcli — Speed Editor smoke-test CLI (vibes/05-prototypes.md task 3)\n"
        "\n"
        "usage: speedcli [command] [options]\n"
        "\n"
        "commands:\n"
        "  scan     list Speed Editors on the USB bus (exit 1 if none)\n"
        "  listen   connect, authenticate, decode inputs, demonstrate outputs (default)\n"
        "  demo     connect, authenticate, chase all 21 LEDs\n"
        "  mode N   connect, authenticate, set wheel mode N (0-3) + jog LED, exit\n"
        "\n"
        "options:\n"
        "  -v       hexdump every raw input report\n"
        "  -h, --help  this help\n"
        "\n"
        "listen: SHTL/JOG/SCRL switch wheel mode and light their LED,\n"
        "        SNAP toggles its main-panel LED, ESC or Ctrl-C exits.\n"
        "\n"
        "exit codes: 0 ok, 1 no device/open failed, 2 auth failed,\n"
        "            3 device write failed, 64 usage error\n"
        "\n"
        "See SMOKE-TEST.md for manual test steps and\n"
        "docs/speed-editor-capabilities.md for the protocol reference.\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args;
    bool help = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-v") { g_verbose = true; }
        else if (a == "-h" || a == "--help") { help = true; }
        else { args.push_back(a); }
    }
    if (help) { return usage(); }
    if (args.empty()) { args.push_back("listen"); }  // default command

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    const std::string& cmd = args[0];
    if (cmd == "scan") { return cmdScan(); }
    if (cmd == "listen") { return cmdListen(); }
    if (cmd == "demo") { return cmdDemo(); }
    if (cmd == "mode") { return cmdMode(args.size() > 1 ? args[1].c_str() : nullptr); }

    std::fprintf(stderr, "unknown command: %s\n\n", cmd.c_str());
    usage();
    return 64;
}
