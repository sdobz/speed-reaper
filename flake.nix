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
        # Windows cross toolchain (vibes/05-prototypes.md task 4): the
        # Speed Editor works on Windows too, so speedcli gets a .exe build.
        # hidapi's winapi backend LoadLibrary()s the system hid.dll at
        # runtime, and we statically link the C++ runtime, so the result
        # is a single self-contained speedcli.exe.
        pkgsWin = import nixpkgs {
          inherit system;
          crossSystem = {
            config = "x86_64-w64-windows-gnu";
            system = "x86_64-pc-windows-gnu";
          };
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
        echo "Build CLI for Windows (task 4): nix build .#speedcli-windows → .../bin/speedcli.exe"
        echo "(REAPER plugin, later tasks: cmake -S src -B build -G Ninja && cmake --build build)"
      '';
    };

    # Static winapi backend (BUILD_SHARED_LIBS=OFF) so speedcli.exe has no
    # DLL dependency other than the Windows-provided hid.dll, which the hidapi
    # winapi backend LoadLibrary()s at runtime.
    hidapiWin = pkgsWin.hidapi.overrideAttrs (final: prev: {
      cmakeFlags = [ "-DBUILD_SHARED_LIBS=OFF" ];
    });

    packages = {
      # Windows cross-build of the speedcli CLI (vibes/05-prototypes.md task 4).
      #   nix build .#speedcli-windows --print-out-paths
      # → .../bin/speedcli.exe — single self-contained file (static C++
      # runtime, static hidapi); no installer, no DLLs to ship alongside.
      speedcli-windows = pkgsWin.stdenv.mkDerivation {
        pname = "speedcli";
        version = "0.1.0";

        src = ./src; # driver + CLI only (keeps vendor/ out of the closure)

        # make itself is a build-host tool: take it from the *native* set.
        nativeBuildInputs = [ pkgs.gnumake ];
        buildInputs = [ hidapiWin ];

        # CROSS=1 selects the Windows link line in src/Makefile (see there);
        # HIDAPI_PREFIX is the cross-compiled static hidapi.
        makeFlags = [ "CROSS=1" "HIDAPI_PREFIX=${hidapiWin}" ];

        installPhase = ''
          mkdir -p $out/bin
          install -m755 ../build/speedcli.exe $out/bin/speedcli.exe
        '';

        meta = {
          description = "Blackmagic Speed Editor control CLI (speedcli), Windows x86-64 cross-build";
          homepage = "https://www.blackmagicdesign.com/products/speededitor";
          license = pkgs.lib.licenses.bsd3; # driver port of John-Howard/PythonSpeedEditor (see src/speed_editor/LICENSE)
          # built on x86_64-linux, artifact targets Windows x86-64
          platforms = pkgs.lib.platforms.windows;
          crossPlatforms = [ "x86_64-linux" ];
          mainProgram = "speedcli.exe";
        };
      };
    };
  }));
}
