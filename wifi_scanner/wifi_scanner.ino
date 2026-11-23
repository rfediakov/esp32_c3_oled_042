#include <Arduino.h>
#include <WiFi.h>
#include <U8g2lib.h>
#include <Wire.h>

// Hardware Pin Definitions for ESP32-C3 0.42" OLED
#define SDA_PIN 5
#define SCL_PIN 6

#define SCREEN_WIDTH 72
#define SCREEN_HEIGHT 40

// Initialize U8g2 for EastRising 0.42" OLED (72x40 SSD1306 I2C)
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(
  U8G2_R0,
  U8X8_PIN_NONE
);

const unsigned long SCAN_INTERVAL = 10000; // Rescan every 10 seconds
const unsigned long PAGE_INTERVAL = 2500;  // Switch page every 2.5 seconds

unsigned long lastScan = 0;
unsigned long lastPage = 0;

int networkCount = 0;
int currentPage = 0;
bool scanning = false;

// Draw signal strength bars
void drawSignal(int x, int y, int rssi) {
  int bars = 0;

  if (rssi >= -50) bars = 4;
  else if (rssi >= -60) bars = 3;
  else if (rssi >= -70) bars = 2;
  else if (rssi >= -85) bars = 1;

  for (int i = 0; i < 4; i++) {
    int h = (i + 1) * 2;

    if (i < bars) {
      u8g2.drawBox(x + i * 3, y - h, 2, h);
    } else {
      u8g2.drawFrame(x + i * 3, y - h, 2, h);
    }
  }
}

// Render Wi-Fi networks on the 72x40 OLED screen
void drawScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);

  // Header
  u8g2.drawStr(0, 7, "WIFI SCANNER");
  u8g2.drawHLine(0, 9, SCREEN_WIDTH);

  if (scanning && networkCount == 0) {
    u8g2.drawStr(2, 25, "Scanning...");
    u8g2.sendBuffer();
    return;
  }

  if (networkCount == 0) {
    u8g2.drawStr(2, 25, "No networks");
    u8g2.sendBuffer();
    return;
  }

  const int perPage = 3;
  int pages = (networkCount + perPage - 1) / perPage;

  if (currentPage >= pages) {
    currentPage = 0;
  }

  int start = currentPage * perPage;

  for (int i = 0; i < perPage; i++) {
    int index = start + i;
    if (index >= networkCount) break;

    int y = 18 + i * 10;

    String ssid = WiFi.SSID(index);
    if (ssid.length() == 0) {
      ssid = "<hidden>";
    }

    // Truncate SSID to fit 72x40 layout
    if (ssid.length() > 8) {
      ssid = ssid.substring(0, 8);
    }

    int rssi = WiFi.RSSI(index);

    u8g2.setCursor(0, y);
    u8g2.print(ssid);

    u8g2.setCursor(43, y);
    u8g2.print(rssi);

    drawSignal(62, y, rssi);
  }

  // Footer status bar
  u8g2.drawHLine(0, 37, SCREEN_WIDTH);

  u8g2.setCursor(0, 40);
  u8g2.print(networkCount);
  u8g2.print(" NET");

  u8g2.setCursor(48, 40);
  u8g2.print(currentPage + 1);
  u8g2.print("/");
  u8g2.print(pages);

  u8g2.sendBuffer();
}

// Start asynchronous Wi-Fi scan
void startScan() {
  Serial.println("Starting WiFi scan...");
  WiFi.scanDelete();

  networkCount = 0;
  currentPage = 0;
  scanning = true;

  drawScreen();

  WiFi.scanNetworks(true, false); // Async scan
  lastScan = millis();
}

// Check completion status of async scan
void checkScan() {
  if (!scanning) return;

  int result = WiFi.scanComplete();

  if (result == WIFI_SCAN_RUNNING) {
    return;
  }

  scanning = false;

  if (result >= 0) {
    networkCount = result;
    Serial.printf("Found %d networks\n", networkCount);

    for (int i = 0; i < networkCount; i++) {
      Serial.printf("%s | %d dBm | CH %d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }
  } else {
    Serial.printf("Scan error: %d\n", result);
    networkCount = 0;
  }

  drawScreen();
}

void setup() {
  Serial.begin(115200);

  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(2, 17, "WIFI");
  u8g2.drawStr(2, 32, "Starting...");
  u8g2.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(500);

  startScan();
}

void loop() {
  checkScan();

  // Rotate pages automatically
  if (!scanning && millis() - lastPage >= PAGE_INTERVAL) {
    lastPage = millis();

    int pages = (networkCount + 2) / 3;
    if (pages > 1) {
      currentPage = (currentPage + 1) % pages;
      drawScreen();
    }
  }

  // Periodic rescan
  if (!scanning && millis() - lastScan >= SCAN_INTERVAL) {
    startScan();
  }

  delay(20);
}
