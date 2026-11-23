# ESP32-C3 0.42" OLED Scanners

Lightweight Wi-Fi and Bluetooth Low Energy (BLE) scanner firmware for the ESP32-C3 with its onboard 0.42-inch monochrome OLED display (72x40 pixels, SSD1306 driver).

---

## 📸 Working Prototype
*(Photos of the working ESP32-C3 board with the OLED display showing scanned networks will be added here soon)*

---

## 📌 Hardware Specifications
- **Microcontroller**: Espressif ESP32-C3 (RISC-V 32-bit single-core, 160 MHz)
- **Wireless**: Wi-Fi 802.11 b/g/n & Bluetooth 5 (LE)
- **Display**: 0.42" OLED (SSD1306 driver, 72x40 resolution, I2C)
- **Flash**: 4 MB Embedded Flash

---

## 🔌 Pinout & Wiring
| Pin on ESP32-C3 | OLED Display Pin |
|-----------------|------------------|
| **GPIO 5**      | SDA              |
| **GPIO 6**      | SCL              |
| **5V / 3.3V**   | VCC              |
| **GND**         | GND              |

---

## 🚀 Wi-Fi Scanner Features
- **Asynchronous Wi-Fi Scanning**: Non-blocking network scanning (`WiFi.scanNetworks(true, false)`) to keep the UI responsive.
- **Signal Strength Indicator**: Dynamic 4-bar RSSI graphical indicator for every detected network.
- **Paged UI**: Automatically cycles through pages of 3 networks at a time with channel and RSSI dBm metrics.

---

## Bluetooth Scanner

The `bluetooth_scanner` sketch periodically scans for nearby BLE advertisements and shows each device name (or address when unnamed) with its signal strength. It displays two devices per page on the OLED and logs the results over serial. It does not connect to or interact with discovered devices.

## 📂 Project Structure
```text
esp32_c3_oled_042/
├── DEVICE_SPEC.md              # Hardware specs & AI agent guidelines
├── FLASHING.md                  # Detailed firmware flashing instructions
├── flash.sh                     # Compile and flash Arduino sketches
├── bluetooth_scanner/
│   └── bluetooth_scanner.ino   # Arduino sketch for BLE scanning & OLED rendering
└── wifi_scanner/
    └── wifi_scanner.ino        # Arduino sketch for Wi-Fi scanning & OLED rendering
```

---

## 🛠️ Getting Started & Flashing
1. Open the desired `.ino` file (`wifi_scanner/wifi_scanner.ino` or `bluetooth_scanner/bluetooth_scanner.ino`) in **Arduino IDE**.
2. Install required library: **U8g2** (by Olikraus) via Library Manager. BLE support is included with the ESP32 Arduino board package.
3. Select board: **ESP32C3 Dev Module** (or `esp32:esp32:esp32c3`).
4. Connect via USB-C and upload!

For detailed terminal flashing instructions, port selection, and troubleshooting, see [FLASHING.md](./FLASHING.md). The quick command is `./flash.sh bluetooth_scanner`.
