#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SDA_PIN 5
#define SCL_PIN 6

#define SCREEN_WIDTH 72

const unsigned long SCAN_INTERVAL = 10000;
const unsigned long PAGE_INTERVAL = 2500;
const uint32_t SCAN_DURATION_SECONDS = 5;
const int MAX_DEVICES = 24;
const int DEVICES_PER_PAGE = 2;

struct NearbyDevice {
  char name[16];
  int rssi;
};

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
BLEScan* scanner = nullptr;
NearbyDevice devices[MAX_DEVICES];

int deviceCount = 0;
int currentPage = 0;
unsigned long lastScan = 0;
unsigned long lastPage = 0;
bool scanning = false;

void drawSignal(int x, int y, int rssi) {
  int bars = 0;
  if (rssi >= -50) bars = 4;
  else if (rssi >= -60) bars = 3;
  else if (rssi >= -70) bars = 2;
  else if (rssi >= -85) bars = 1;

  for (int i = 0; i < 4; i++) {
    const int height = (i + 1) * 2;
    if (i < bars) {
      u8g2.drawBox(x + i * 3, y - height, 2, height);
    } else {
      u8g2.drawFrame(x + i * 3, y - height, 2, height);
    }
  }
}

void drawScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  u8g2.drawStr(0, 7, "BLE SCANNER");
  u8g2.drawHLine(0, 9, SCREEN_WIDTH);

  if (scanning) {
    u8g2.drawStr(2, 25, "Scanning...");
    u8g2.sendBuffer();
    return;
  }

  if (deviceCount == 0) {
    u8g2.drawStr(2, 25, "No devices");
    u8g2.drawHLine(0, 32, SCREEN_WIDTH);
    u8g2.drawStr(0, 39, "0 DEV");
    u8g2.sendBuffer();
    return;
  }

  const int pageCount = (deviceCount + DEVICES_PER_PAGE - 1) / DEVICES_PER_PAGE;
  if (currentPage >= pageCount) currentPage = 0;
  const int start = currentPage * DEVICES_PER_PAGE;

  for (int row = 0; row < DEVICES_PER_PAGE; row++) {
    const int index = start + row;
    if (index >= deviceCount) break;

    const int y = 18 + row * 10;
    u8g2.setCursor(0, y);
    u8g2.print(devices[index].name);
    u8g2.setCursor(38, y);
    u8g2.print(devices[index].rssi);
    drawSignal(60, y, devices[index].rssi);
  }

  u8g2.drawHLine(0, 32, SCREEN_WIDTH);
  u8g2.setCursor(0, 39);
  u8g2.print(deviceCount);
  u8g2.print(" DEV");
  u8g2.setCursor(49, 39);
  u8g2.print(currentPage + 1);
  u8g2.print("/");
  u8g2.print(pageCount);
  u8g2.sendBuffer();
}

void scanDevices() {
  Serial.println("Starting BLE scan...");
  scanning = true;
  drawScreen();

  scanner->clearResults();
  BLEScanResults* results = scanner->start(SCAN_DURATION_SECONDS, false);

  deviceCount = min(static_cast<int>(results->getCount()), MAX_DEVICES);
  for (int i = 0; i < deviceCount; i++) {
    BLEAdvertisedDevice device = results->getDevice(i);
    String label = device.haveName()
      ? device.getName()
      : device.getAddress().toString();
    if (label.length() > sizeof(devices[i].name) - 1) {
      label = label.substring(0, sizeof(devices[i].name) - 1);
    }
    label.toCharArray(devices[i].name, sizeof(devices[i].name));
    devices[i].rssi = device.getRSSI();

    Serial.printf("%s | %d dBm\n", devices[i].name, devices[i].rssi);
  }

  scanner->clearResults();
  currentPage = 0;
  scanning = false;
  lastScan = millis();
  lastPage = lastScan;
  drawScreen();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  u8g2.setFont(u8g2_font_6x12_tf);
  u8g2.drawStr(2, 17, "BLE");
  u8g2.drawStr(2, 32, "Starting...");
  u8g2.sendBuffer();

  BLEDevice::init("");
  scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(99);

  scanDevices();
}

void loop() {
  if (deviceCount > DEVICES_PER_PAGE && millis() - lastPage >= PAGE_INTERVAL) {
    lastPage = millis();
    const int pageCount = (deviceCount + DEVICES_PER_PAGE - 1) / DEVICES_PER_PAGE;
    currentPage = (currentPage + 1) % pageCount;
    drawScreen();
  }

  if (millis() - lastScan >= SCAN_INTERVAL) {
    scanDevices();
  }

  delay(20);
}
