PORT := "/dev/ttyACM0"
BAUD := "9600"
FQBN := "arduino:avr:mega"
SKETCH := "arduino/mega2560_firmware"

board:
	arduino-cli board list

arduino-init:
	arduino-cli core update-index
	arduino-cli core install arduino:avr
	arduino-cli lib install "DHT sensor library"
	arduino-cli lib install "Adafruit Unified Sensor"

arduino-compile:
	arduino-cli compile --fqbn {{FQBN}} {{SKETCH}}

upload:
	arduino-cli compile -u -p {{PORT}} --fqbn {{FQBN}} {{SKETCH}}

monitor:
	arduino-cli monitor -p {{PORT}} -c baudrate={{BAUD}}

build:
	make

gui:
	./build/projetbadis_gui --serial {{PORT}} --baud {{BAUD}}

clean:
	rm -rf build/*
