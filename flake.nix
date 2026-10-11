{
  description = "REAPER extension driving transport from the Blackmagic Speed Editor";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, utils }:
    (utils.lib.eachSystem ["x86_64-linux" ] (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true; # reaper binary for manual testing
        };
  in rec {
    devShell = pkgs.mkShell {
      buildInputs = with pkgs; [
        # build toolchain (see docs/reaper-dev-env.md §8)
        cmake
        ninja
        gcc
        gnumake
        pkg-config
        git
        gdb
        valgrind

        # SWELL on Linux resolves gdk-3.0 via pkg-config at build time
        gtk3

        # the Speed Editor is a USB HID device (hidraw backend built in on Linux)
        hidapi
        libusb1
        udev

        # REAPER binary for manual testing (nixpkgs has reaper-7.81)
        reaper
      ];
      shellHook = ''
        echo "Welcome to the speed-reaper development environment!"
        echo "  g++      $(g++ --version 2>/dev/null | head -1)"
        echo "  cmake    $(cmake --version 2>/dev/null | head -1)"
        echo "  reaper   $(reaper --version 2>/dev/null || echo 'not on PATH')"
        echo "SDK include roots: -Ivendor/reaper-sdk/sdk -Ivendor/WDL/WDL (see docs/reaper-dev-env.md §2)"
        echo "Build CLI (task 3): make -C src            → build/speedcli"
        echo "(REAPER plugin, later tasks: cmake -S src -B build -G Ninja && cmake --build build)"
      '';
    };
  }));
}
