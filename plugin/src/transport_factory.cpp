// MakeTransport. See the header.
#include "transport_factory.h"

#include "speed_editor/hidapi_transport.h"

#ifdef SPEED_REAPER_FAKE_DEVICE
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "speed_editor/fake_transport.h"
#endif

std::unique_ptr<IHidTransport> MakeTransport() {
#ifdef SPEED_REAPER_FAKE_DEVICE
  if (const char *path = std::getenv("SPEED_REAPER_FAKE_DEVICE")) {
    if (*path != '\0') {
      std::ifstream in(path);
      std::ostringstream ss;
      ss << in.rdbuf();
      std::string err;
      auto evs = ParseFakeScript(ss.str(), &err);
      if (!in) {
        std::fprintf(stderr,
                     "transport_factory: cannot read fake script %s (using an "
                     "empty script)\n",
                     path);
      } else if (!err.empty()) {
        std::fprintf(stderr, "transport_factory: bad fake script %s: %s (using "
                             "an empty script)\n",
                     path, err.c_str());
      }
      return std::make_unique<FakeTransport>(std::move(evs));
    }
  }
#endif
  return std::make_unique<HidapiTransport>();
}
