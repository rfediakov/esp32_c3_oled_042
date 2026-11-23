# ESP32-C3 0.42" OLED Tiny Board Documentation

## Hardware Overview
- **Microcontroller**: Espressif ESP32-C3 (QFN32, Revision v0.4)
- **Core Architecture**: 32-bit RISC-V Single-Core microprocessor
- **Clock Speed**: Up to 160 MHz
- **Wireless**: Wi-Fi (802.11 b/g/n) + Bluetooth 5 (LE)
- **Flash Memory**: 4 MB Embedded Flash (XMC)
- **USB Interface**: Native USB-Serial / JTAG Controller (CDC/ACM)
- **Built-in Display**: 0.42-inch Monochrome OLED (SSD1306 driver, 72x40 pixels)

---

## Pinout & Wiring Map

| Pin | Function | Description |
|---|---|---|
| **GPIO 5** | SDA | I2C Data line connected to the built-in 0.42" OLED display |
| **GPIO 6** | SCL | I2C Clock line connected to the built-in 0.42" OLED display |
| **USB** | D+/D- | Native USB-Serial/JTAG interface (Port: `/dev/cu.usbmodem3101` on macOS) |

---

## Display Configuration (`U8g2lib`)
The onboard 0.42" OLED uses an SSD1306 controller with a non-standard resolution of **72x40 pixels**. 
Initialization in Arduino:
```cpp
#include <U8g2lib.h>
#include <Wire.h>

#define SDA_PIN 5
#define SCL_PIN 6

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
```

---

## Guidelines for AI Agents & Developers
1. **I2C Initialization**: Always call `Wire.begin(SDA_PIN, SCL_PIN);` before `u8g2.begin();`.
2. **Display Constraints**: Screen dimensions are strictly 72 pixels wide by 40 pixels high. Font selection is critical (use compact fonts like `u8g2_font_5x7_tf` or `u8g2_font_6x12_tf`).
3. **Wi-Fi Scanning**: Use asynchronous scanning (`WiFi.scanNetworks(true, false)`) to prevent blocking the UI loop and display responsiveness.
4. **Board FQBN for CLI**: `esp32:esp32:esp32_family` (or `esp32:esp32:esp32c3`).
