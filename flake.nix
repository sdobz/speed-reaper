{
  description = "REAPER extension driving transport from the Blackmagic Speed Editor";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      linuxSystem = "x86_64-linux";
      winSystem = "x86_64-pc-windows-gnu";

      pkgs = import nixpkgs {
        system = linuxSystem;
        config.allowUnfree = true; # reaper binary for manual testing
      };
      # Windows cross toolchain (vibes/05-prototypes.md task 4): the
      # Speed Editor works on Windows too, so speedcli gets a .exe build.
      # hidapi's winapi backend LoadLibrary()s the system hid.dll at
      # runtime, and we statically link the C++ runtime, so the result
      # is a single self-contained speedcli.exe.
      #
      # Uses nixpkgs' standard mingw triple (x86_64-w64-mingw32); the
      # non-standard "x86_64-w64-windows-gnu" is rejected by config.sub.
      pkgsWin = pkgs.pkgsCross.mingwW64;

      # Static winapi backend (BUILD_SHARED_LIBS=OFF) so speedcli.exe has no
      # DLL dependency other than the Windows-provided hid.dll, which the hidapi
      # winapi backend LoadLibrary()s at runtime.
      hidapiWin = pkgsWin.hidapi.overrideAttrs (final: prev: {
        cmakeFlags = [ "-DBUILD_SHARED_LIBS=OFF" ];
      });

      # One derivation per platform — same src, same Makefile.
      # cross=false: native x86_64-linux build against nixpkgs' shared
      #   hidapi (hidraw backend) → bin/speedcli
      # cross=true:  mingwW64 cross-build against static hidapi (winapi
      #   backend) with CROSS=1 → bin/speedcli.exe (self-contained)
      makeSpeedcli = { spkgs, hidapi, cross, targetSystem }:
        spkgs.stdenv.mkDerivation {
          pname = "speedcli";
          version = "0.1.0";

          src = ./src; # driver + CLI only (keeps vendor/ out of the closure)

          # make itself is a build-host tool.
          nativeBuildInputs = [ spkgs.gnumake ];
          buildInputs = [ hidapi ];

          # HIDAPI_PREFIX points the Makefile at the (possibly cross-compiled)
          # hidapi output; CROSS=1 selects the Windows link line (see src/Makefile).
          makeFlags =
            [ "HIDAPI_PREFIX=${hidapi}" ]
            ++ pkgs.lib.optionals cross [ "CROSS=1" ];

          installPhase = ''
            mkdir -p $out/bin
            ${
              if cross
              then "install -m755 ../build/speedcli.exe $out/bin/speedcli.exe"
              else "install -m755 ../build/speedcli $out/bin/speedcli"
            }
          '';

          meta = {
            description = "Blackmagic Speed Editor control CLI (speedcli)";
            homepage = "https://www.blackmagicdesign.com/products/speededitor";
            license = pkgs.lib.licenses.bsd3; # driver port of John-Howard/PythonSpeedEditor (see src/speed_editor/LICENSE)
            # check-meta compares stdenv.hostPlatform.system (the *elaborated*
            # target, e.g. "x86_64-windows") against meta.platforms with plain
            # string equality — for cross builds both spellings are required.
            platforms =
              if cross
              then [ targetSystem (pkgs.lib.systems.elaborate targetSystem).system ]
              else [ targetSystem ];
            # built on x86_64-linux, artifact targets Windows x86-64
            crossPlatforms = pkgs.lib.optionals cross [ linuxSystem ];
            mainProgram =
              if cross then "speedcli.exe" else "speedcli";
          };
        };
      # REAPER plugin cross-build (vibes/05-prototypes.md task 7): same
      # CMake source as the Linux dev-shell build, compiled with mingwW64
      # → self-contained reaper_speed_reaper.dll (static C++ runtime).
      # MinGW works because the extension's REAPER surface is C-ABI only
      # (see CROSS-PLATFORM.md at the repo root).
      makeSpeedReaperPlugin = { spkgs, targetSystem, cross, artifactSuffix }:
        spkgs.stdenv.mkDerivation {
          pname = "reaper-speed-reaper";
          version = "0.1.0";

          # Whole repo: plugin/CMakeLists.txt pulls vendor/reaper-sdk +
          # vendor/WDL from the repo root via REPO_ROOT.
          src = ./.;

          nativeBuildInputs = [ spkgs.cmake spkgs.ninja ];

          # The CMakeLists.txt is in plugin/ (not the source root), and the
          # whole repo is the source (CMakeLists pulls vendor/ via REPO_ROOT).
          # The stdenv cmake hook's default configurePhase would run
          # `cmake` against the source root and fail, so both phases are
          # overridden explicitly.
          configurePhase = ''
            # $cmakeFlags carries the stdenv cross-compiler +
            # -DCMAKE_SYSTEM_NAME=Windows settings; without them CMake
            # misdetects the platform as Unix (host) and the pkg-config/
            # gdk branch fires.
            cmake $cmakeFlags -S plugin -B build -G Ninja
          '';

          buildPhase = ''
            cmake --build build
          '';

          # ''…'' not """…""": the """ multiline-string syntax was
          # removed in newer Nix (2.34 here) — """ now mis-parses as "" "…".
          installPhase = ''
            mkdir -p $out/reaper
            install -m644 build/reaper_speed_reaper${artifactSuffix} $out/reaper/
          '';

          meta = {
            description = "Speed Reaper REAPER extension (smooth playhead jog demo)";
            homepage = "https://www.reaper.fm/sdk/plugin/plugin.php";
            # Same check-meta quirk as speedcli: cross builds need both the
            # triple and the elaborated target spellings.
            platforms =
              if cross
              then [ targetSystem (pkgs.lib.systems.elaborate targetSystem).system ]
              else [ targetSystem ];
            crossPlatforms = pkgs.lib.optionals cross [ linuxSystem ];
          };
        };
    in
    {
      devShells.${linuxSystem}.default = pkgs.mkShell {
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
          echo "Build CLI (task 3): make -C src                → build/speedcli"
          echo "Build CLI (task 5): nix build .#packages.x86_64-linux.speedcli              → .../bin/speedcli"
          echo "                   nix build .#packages.x86_64-pc-windows-gnu.speedcli → .../bin/speedcli.exe"
          echo "Build REAPER plugin (task 6): cmake -S plugin -B build/plugin -G Ninja && cmake --build build/plugin"
          echo "  → build/plugin/reaper_speed_reaper.so (install: see SMOKE-TEST.md, 'REAPER plugin' section)"
          echo "Build REAPER plugin for Windows (task 7): nix build .#reaper-plugin-windows → .../reaper/reaper_speed_reaper.dll"
          echo "  Cross-platform matrix + gotchas: CROSS-PLATFORM.md"
        '';
      };

      # Linux/Windows parity (vibes/05-prototypes.md task 5): both systems
      # expose the same package name, built from the same src via the
      # Makefile's native / CROSS=1 paths.
      packages.${linuxSystem} = {
        speedcli = makeSpeedcli {
          spkgs = pkgs;
          hidapi = pkgs.hidapi;
          cross = false;
          targetSystem = linuxSystem;
        };
        # Compat alias for task 4's `nix build .#speedcli-windows`.
        speedcli-windows = self.packages.${winSystem}.speedcli;
        # Cross-platform REAPER plugin (task 7, see CROSS-PLATFORM.md).
        reaper-plugin-windows = self.packages.${winSystem}.reaper-plugin;
      };

      packages.${winSystem} = {
        speedcli = makeSpeedcli {
          spkgs = pkgsWin;
          hidapi = hidapiWin;
          cross = true;
          targetSystem = winSystem;
        };
        # Windows parity build of the REAPER plugin (task 7).
        reaper-plugin = makeSpeedReaperPlugin {
          spkgs = pkgsWin;
          targetSystem = winSystem;
          cross = true;
          artifactSuffix = ".dll";
        };
      };

      # `defaultPackages` is not a flake output (nix flake check warns about it);
      # the standard per-system default is `defaultPackage`.
      defaultPackage.${linuxSystem} = self.packages.${linuxSystem}.speedcli;
      defaultPackage.${winSystem} = self.packages.${winSystem}.speedcli;
    };
}
