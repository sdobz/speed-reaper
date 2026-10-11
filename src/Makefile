# Build the speedcli smoke-test CLI (vibes/05-prototypes.md task 3).
#
# Usage (from the nix dev env — `direnv allow` / `nix develop`):
#   make -C src            # → build/speedcli
#   make -C src clean
#
# Requires: g++, pkg-config, the hidapi-hidraw pkg-config module
# (all provided by flake.nix devShell).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
HIDAPI_CFLAGS := $(shell pkg-config --cflags hidapi-hidraw)
HIDAPI_LIBS   := $(shell pkg-config --libs hidapi-hidraw)

BUILD_DIR := ../build
CLI       := $(BUILD_DIR)/speedcli
SRCS      := cli/speedcli.cpp speed_editor/hid_device.cpp

.PHONY: all clean
all: $(CLI)

$(CLI): $(SRCS) speed_editor/hid_device.h speed_editor/key_map.h speed_editor/event_types.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. $(HIDAPI_CFLAGS) $(SRCS) -o $@ $(HIDAPI_LIBS)
	@echo "built $@"

clean:
	rm -f $(CLI)
