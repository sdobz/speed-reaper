# Build the speedcli smoke-test CLI (vibes/05-prototypes.md tasks 3–4).
#
# Usage (from the nix dev env — `direnv allow` / `nix develop`):
#   make -C src            # → build/speedcli
#   make -C src clean
#
# Requires: g++, pkg-config, the hidapi-hidraw pkg-config module
# (all provided by flake.nix devShell).
#
# Flakified (vibes task 5, parity for both platforms) — driven by flake.nix:
#   nix build .#x86_64-linux.speedcli            → .../bin/speedcli
#   nix build .#x86_64-pc-windows-gnu.speedcli   → .../bin/speedcli.exe
# Equivalent to by hand (Windows):
#   make -C src CROSS=1 CXX=x86_64-w64-mingw32-g++ HIDAPI_PREFIX=<hidapi-out>
#   → build/speedcli.exe (fully self-contained: static libstdc++/libgcc,
#     static hidapi winapi backend that LoadLibrary()s the system hid.dll).

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
BUILD_DIR := ../build
SRCS      := cli/speedcli.cpp speed_editor/hid_device.cpp speed_editor/hidapi_transport.cpp

ifeq ($(CROSS),1)
# Windows cross-build: HIDAPI_PREFIX is the (cross-compiled) hidapi output
# with include/hidapi/hidapi.h and lib/libhidapi.a (static winapi backend).
CLI        := $(BUILD_DIR)/speedcli.exe
HIDAPI_LIB  ?= hidapi
HIDAPI_CFLAGS := -I$(HIDAPI_PREFIX)/include
HIDAPI_LIBS   := -L$(HIDAPI_PREFIX)/lib -l$(HIDAPI_LIB)
# Fully self-contained exe (no libstdc++.dll / libgcc dll needed on the target):
EXTRA_LDFLAGS := -static
else
CLI        := $(BUILD_DIR)/speedcli
# nixpkgs' hidapi installs per-backend libraries (libhidapi-hidraw.so,
# libhidapi-libusb.so) — no generic libhidapi.so — so link the backend.
HIDAPI_LIB  ?= hidapi-hidraw
ifneq ($(HIDAPI_PREFIX),)
# flake.nix passes the store path of nixpkgs' hidapi directly.
HIDAPI_CFLAGS := -I$(HIDAPI_PREFIX)/include
HIDAPI_LIBS   := -L$(HIDAPI_PREFIX)/lib -l$(HIDAPI_LIB)
else
HIDAPI_CFLAGS := $(shell pkg-config --cflags $(HIDAPI_LIB))
HIDAPI_LIBS   := $(shell pkg-config --libs $(HIDAPI_LIB))
endif
endif

.PHONY: all clean
all: $(CLI)

$(CLI): $(SRCS) speed_editor/hid_device.h speed_editor/hid_transport.h \
	       speed_editor/hidapi_transport.h speed_editor/key_map.h speed_editor/event_types.h
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I. $(HIDAPI_CFLAGS) $(SRCS) -o $@ $(HIDAPI_LIBS) $(EXTRA_LDFLAGS)
	@echo "built $@"

clean:
	rm -f $(BUILD_DIR)/speedcli $(BUILD_DIR)/speedcli.exe
