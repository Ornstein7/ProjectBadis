{
  description = "Arduino Mega 2560 + DHT22 + C GTK GUI project";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems
          (system: f nixpkgs.legacyPackages.${system});
    in {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = with pkgs; [
            arduino-cli
            avrdude

            gcc
            gnumake
            pkg-config
            gtk4
            glib

            just
            tio
          ];

          shellHook = ''
            export ARDUINO_DIRECTORIES_DATA="$PWD/.arduino15"
            export ARDUINO_DIRECTORIES_USER="$PWD/arduino"
            export ARDUINO_DIRECTORIES_DOWNLOADS="$PWD/.arduino15/staging"

            echo "ProjetBadis GTK/Arduino dev shell ready"
            echo "Run: make"
            echo "Run: make run"
            echo "Run: just arduino-init"
          '';
        };
      });
    };
}
