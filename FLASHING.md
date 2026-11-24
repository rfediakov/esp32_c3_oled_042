# Flashing Firmware

This guide covers flashing the ESP32-C3 OLED board with the provided sketches or another Arduino sketch.

## Requirements

- The ESP32-C3 connected to the computer with a USB data cable.
- Arduino IDE (which bundles Arduino CLI) or `arduino-cli` available on your `PATH`.
- The Espressif ESP32 board package installed, including the `esp32:esp32:esp32c3` board.
- The U8g2 library installed through Arduino Library Manager for the supplied sketches.
- Python 3 for automatic USB serial-port detection.

The script checks for `arduino-cli` on `PATH` first. On macOS, if it is not on `PATH`, it also checks the Arduino IDE app's bundled CLI location.

## Flash a project sketch

From the repository root, connect the board and run:

```sh
./flash.sh bluetooth_scanner
```

For the Wi-Fi scanner, run:

```sh
./flash.sh wifi_scanner
```

For the combined dashboard and scanner app, run:

```sh
./flash.sh esp32_field_hub
```

The Field Hub uses the ESP32 `min_spiffs` partition layout (1.9 MB app with
OTA support) because the combined BLE, Wi-Fi, OLED, and web interface firmware
does not fit in the default 1.2 MB app partition. The helper selects this
partition automatically for `esp32_field_hub`. Other sketches retain the
board's default partition layout.

The Field Hub creates an open, password-free Wi-Fi access point and an
unauthenticated dashboard. The serial monitor at 115200 baud prints the
network name. Join without entering a password and open
`http://192.168.4.1/`. Anyone nearby can access scan results and change device
settings. See the project README for details.

The script compiles the sketch and then uploads it. By default it uses the ESP32-C3 Dev Module (`esp32:esp32:esp32c3`) and automatically selects a USB serial port when exactly one is detected.

## Select a port or board

If more than one USB serial device is connected, list available ports:

```sh
arduino-cli board list
```

Then specify the port:

```sh
./flash.sh --port /dev/cu.usbmodem3101 bluetooth_scanner
```

To target a different Arduino-compatible board, pass its FQBN:

```sh
./flash.sh --fqbn arduino:avr:uno other_firmware
```

To override the ESP32 partition scheme, pass its Arduino menu option name:

```sh
./flash.sh --partition-scheme min_spiffs esp32_field_hub
```

The sketch folder must contain an `.ino` file with the same name as the folder. You can pass either the folder or that `.ino` file to the script.

## Compile without uploading

With the board connected, run:

```sh
./flash.sh --compile-only bluetooth_scanner
```

This verifies the build and port detection but skips upload.

## Troubleshooting

- **`arduino-cli not found`**: Install Arduino IDE or Arduino CLI. If using a CLI installed outside the IDE, add it to `PATH` or set `ARDUINO_CLI` to its executable path.
- **`No USB serial board detected`**: Reconnect the board with a USB data cable, then run `arduino-cli board list`. You can also pass the detected port with `--port`.
- **Multiple USB serial boards detected**: Specify which board to use with `--port`.
- **Missing ESP32 board or U8g2 library**: Install the ESP32 board package and U8g2 library, then rerun the script.
- **Upload fails**: Confirm the selected FQBN matches the connected board and close any application that may be using the serial port.
