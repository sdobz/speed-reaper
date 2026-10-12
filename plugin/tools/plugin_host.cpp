// plugin_host — a stand-in for REAPER with no GUI (Linux).
//
// Docs: docs/plugin-oracle-design.md §3. Loads the plugin .so, feeds it a
// fake reaper_plugin_info_t whose GetFunc() returns a generic (never-called)
// stub iff the name is in the REAPER 7.81 export list
// (oracle/reaper_api_names.txt, overridable via env ORACLE_API_NAMES) and
// nullptr otherwise — reproducing real REAPER's behavior for missing API
// functions. Calls ReaperPluginEntry, runs each registered timer N times,
// then unloads.
//
// Usage:
//   plugin_host <plugin.so> [--ticks N] [--expect-entry 0|1]
//                            [--expect-actions id1,id2] [--report-only]
//
// Exit 0 if no crash and expectations (if any) match; --report-only: exit 0
// unless the host itself failed (bad args, unreadable name list, dlopen /
// symbol error).

#include <dlfcn.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

// Same layout as the SDK's reaper_plugin_info_t
// (vendor/reaper-sdk/sdk/reaper_plugin.h): caller_version, hwnd_main,
// Register, GetFunc. On Linux HWND is void*. Register is declared variadic
// because REAPER's actual plugin_register is variadic; the SDK header's
// two-arg form is what the plugin casts around in plugin/src/main.cpp.
// ("timer" takes just the callback: REAPER chooses the ~30 Hz period.)
struct reaper_plugin_info_t {
  int caller_version;
  void* hwnd_main;
  int (*Register)(const char* name, ...);
  void* (*GetFunc)(const char* name);
};

// custom_action_register_t (reaper_plugin.h) — the struct the plugin points
// its Register("custom_action", &ca) at.
struct CaReg {
  int command;
  const char* id_str;
  const char* name;
  const char* menu_name;
};

struct ActionReg {
  int cmd;
  std::string id_str;
};
struct TimerReg {
  int id;
  int interval_ms;
  void (*fn)();
};

struct Host {
  std::unordered_set<std::string> api_names;
  std::vector<std::string> registered;
  std::vector<ActionReg> actions;
  std::vector<TimerReg> timers;
  int next_action_cmd = 40001;
};

Host g;

std::string Join(const std::vector<std::string>& v) {
  std::string out;
  for (const auto& s : v) {
    if (!out.empty()) out += ",";
    out += s;
  }
  return out;
}

int hostRegister(const char* name, ...) {
  std::string n = name ? name : "";
  g.registered.push_back(n);
  va_list ap;
  va_start(ap, name);
  if (n == "timer") {
    // SDK: "timer" takes only void (*)(); REAPER picks the period (~30 Hz).
    void (*fn)() = va_arg(ap, void (*)());
    g.timers.push_back({0, 0, fn});
    va_end(ap);
    return 1;
  }
  if (n == "custom_action") {
    const CaReg* ca = va_arg(ap, const CaReg*);
    va_end(ap);
    if (!ca || !ca->id_str) return 0;
    g.actions.push_back({g.next_action_cmd++, ca->id_str});
    return g.actions.back().cmd;
  }
  va_end(ap);
  return 1;
}

// The one generic stub every name in the 7.81 export list resolves to.
// Never called: the plugin only invokes APIs it actually uses, and today
// entry=0 means no timers/actions are registered (design §3).
extern "C" void reaper_api_stub(void) {}

void* hostGetFunc(const char* name) {
  if (!name) return nullptr;
  return g.api_names.count(name) ? (void*)reaper_api_stub : nullptr;
}

} // namespace

int main(int argc, char** argv) {
  std::string path;
  int ticks = 50;
  int expect_entry = -1;
  std::string expect_actions;
  bool report_only = false;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    auto next = [&]() -> const char* {
      if (i + 1 >= argc) {
        fprintf(stderr, "plugin_host: missing value for %s\n", a.c_str());
        exit(2);
      }
      return argv[++i];
    };
    if (a == "--ticks") ticks = std::atoi(next());
    else if (a == "--expect-entry") expect_entry = std::atoi(next());
    else if (a == "--expect-actions") expect_actions = next();
    else if (a == "--report-only") report_only = true;
    else if (!a.empty() && a[0] == '-') {
      fprintf(stderr, "plugin_host: unknown option %s\n", a.c_str());
      return 2;
    } else path = a;
  }
  if (path.empty()) {
    fprintf(stderr,
            "usage: plugin_host <plugin.so> [--ticks N] [--expect-entry 0|1] "
            "[--expect-actions id1,id2] [--report-only]\n");
    return 2;
  }

  const char* names_env = std::getenv("ORACLE_API_NAMES");
  std::string names_file =
      names_env ? names_env : "oracle/reaper_api_names.txt";
  {
    std::ifstream in(names_file);
    if (!in) {
      fprintf(stderr, "plugin_host: cannot read API name list: %s\n",
              names_file.c_str());
      return 1;
    }
    std::string line;
    while (std::getline(in, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' ' ||
                               line.back() == '\t'))
        line.pop_back();
      if (!line.empty()) g.api_names.insert(line);
    }
  }
  printf("api_names=%s (%zu)\n", names_file.c_str(), g.api_names.size());

  void* inst = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!inst) {
    fprintf(stderr, "plugin_host: dlopen: %s\n", dlerror());
    return 1;
  }
  printf("loaded\n");

  using EntryFn = int (*)(void*, reaper_plugin_info_t*);
  EntryFn entry = (EntryFn)dlsym(inst, "ReaperPluginEntry");
  if (!entry) {
    fprintf(stderr, "plugin_host: no ReaperPluginEntry: %s\n", dlerror());
    return 1;
  }

  reaper_plugin_info_t rec;
  rec.caller_version = 0x20E; // REAPER_PLUGIN_VERSION
  rec.hwnd_main = nullptr;
  rec.Register = hostRegister;
  rec.GetFunc = hostGetFunc;

  int result = entry(inst, &rec);
  printf("entry=%d registered=%s\n", result, Join(g.registered).c_str());

  {
    std::vector<std::string> as;
    for (const auto& a : g.actions)
      as.push_back(std::to_string(a.cmd) + ":" + a.id_str);
    printf("actions=%s\n", Join(as).c_str());
    std::vector<std::string> ts;
    for (const auto& t : g.timers)
      ts.push_back(std::to_string(t.id) + "@" + std::to_string(t.interval_ms) +
                   "ms");
    printf("timers=%s\n", Join(ts).c_str());
  }

  for (const auto& t : g.timers)
    for (int i = 0; i < ticks; ++i) t.fn();
  printf("ticks=%d\n", ticks);

  entry(inst, nullptr); // unload
  printf("unloaded\n");

  int rc = 0;
  if (expect_entry >= 0 && expect_entry != result) {
    fprintf(stderr, "plugin_host: expected entry=%d, got %d\n", expect_entry,
            result);
    rc = 1;
  }
  if (!expect_actions.empty()) {
    std::string tok;
    for (const char* p = expect_actions.c_str(); *p; ++p) {
      if (*p == ',') {
        if (!tok.empty()) {
          bool ok = false;
          for (const auto& a : g.actions)
            if (tok == a.id_str || tok == std::to_string(a.cmd)) ok = true;
          if (!ok) {
            fprintf(stderr, "plugin_host: expected action %s not registered\n",
                    tok.c_str());
            rc = 1;
          }
          tok.clear();
        }
      } else tok += *p;
    }
    if (!tok.empty()) {
      bool ok = false;
      for (const auto& a : g.actions)
        if (tok == a.id_str || tok == std::to_string(a.cmd)) ok = true;
      if (!ok) {
        fprintf(stderr, "plugin_host: expected action %s not registered\n",
                tok.c_str());
        rc = 1;
      }
    }
  }
  if (report_only) return 0; // exit 0 unless the host itself failed
  return rc;
}
