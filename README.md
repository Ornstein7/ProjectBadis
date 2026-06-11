# Debian Setup Without Nix

If Debian does not have Nix, install the normal Debian dependencies:

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-4-dev make git

Then build and run the GUI:

make
./build/projetbadis_gui --serial /dev/ttyACM0 --baud 9600

For Arduino CLI, Nix is much cleaner. So if possible, use the flake.
