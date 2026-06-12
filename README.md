# ProjectBadis

Interface graphique GTK4 en C pour communiquer avec une carte Arduino Mega 2560 équipée d’un capteur DHT22.

Le projet contient deux parties :

* une application GUI C/GTK4 dans `src/` ;
* un firmware Arduino dans `arduino/mega2560_firmware/`.

## Prérequis

### Debian / Ubuntu

```bash
sudo apt update
sudo apt install -y build-essential pkg-config libgtk-4-dev make git arduino-cli
```

### Fedora

```bash
sudo dnf install -y gcc make pkgconf-pkg-config gtk4-devel git arduino-cli
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel pkgconf gtk4 git arduino-cli
```

### Nix

```bash
nix develop
```

Le shell Nix installe les outils nécessaires : compilateur C, `make`, `pkg-config`, GTK4, `arduino-cli`, `avrdude`, `just` et `tio`.

## Compilation de l’interface graphique

Depuis la racine du dépôt :

```bash
make
```

Le binaire est généré ici :

```bash
./build/projetbadis_gui
```

Pour lancer l’application :

```bash
make run
```

Par défaut, le port série est `/dev/ttyACM0` et le baudrate est `9600`.

Pour utiliser un autre port :

```bash
make run PORT=/dev/ttyUSB0 BAUD=9600
```

Ou directement :

```bash
./build/projetbadis_gui --serial /dev/ttyUSB0 --baud 9600
```

## Nettoyage

```bash
make clean
```

## Compilation du firmware Arduino

Initialiser les dépendances Arduino :

```bash
arduino-cli core update-index
arduino-cli core install arduino:avr
arduino-cli lib install "DHT sensor library"
arduino-cli lib install "Adafruit Unified Sensor"
```

Compiler le firmware :

```bash
arduino-cli compile --fqbn arduino:avr:mega arduino/mega2560_firmware
```

Téléverser vers l’Arduino Mega 2560 :

```bash
arduino-cli compile -u -p /dev/ttyACM0 --fqbn arduino:avr:mega arduino/mega2560_firmware
```

Adapte `/dev/ttyACM0` si ta carte utilise un autre port.

## Utilisation avec `just`

Le dépôt fournit aussi un `justfile`.

```bash
just arduino-init
just arduino-compile
just upload
just build
just gui
```

Les valeurs par défaut sont :

```text
PORT=/dev/ttyACM0
BAUD=9600
FQBN=arduino:avr:mega
SKETCH=arduino/mega2560_firmware
```

## Notes de portabilité

* Le projet est prévu pour Linux, car la communication série utilise `termios`.
* L’interface graphique dépend de GTK4.
* Le `Makefile` accepte les variables `CC`, `PKG_CONFIG`, `CFLAGS`, `LDFLAGS`, `PORT` et `BAUD`, ce qui permet d’adapter la compilation à une autre machine.
* Les fichiers générés sont ignorés par Git via `.gitignore` : `build/`, `.arduino15/`, fichiers `.elf`, `.hex`, etc.
  ::: 
