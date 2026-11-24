#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <esp_system.h>

#include "web_ui.h"

namespace {

constexpr uint8_t SDA_PIN = 5;
constexpr uint8_t SCL_PIN = 6;
constexpr uint8_t SCREEN_WIDTH = 72;
constexpr uint8_t SCREEN_HEIGHT = 40;
constexpr uint8_t MAX_WIFI_NETWORKS = 32;
constexpr uint8_t MAX_BLE_DEVICES = 24;
constexpr uint8_t BLE_NAME_LENGTH = 24;
constexpr uint8_t BLE_ADDRESS_LENGTH = 18;
constexpr uint32_t DEFAULT_WIFI_INTERVAL_SECONDS = 30;
constexpr uint32_t DEFAULT_BLE_INTERVAL_SECONDS = 60;
constexpr uint32_t DEFAULT_BLE_DURATION_SECONDS = 4;
constexpr uint32_t DEFAULT_PAGE_INTERVAL_MS = 3500;
constexpr uint32_t DISPLAY_REFRESH_MS = 500;

struct WifiNetwork {
  String ssid;
  int32_t rssi;
  int32_t channel;
  bool secured;
};

struct BleDevice {
  char name[BLE_NAME_LENGTH];
  char address[BLE_ADDRESS_LENGTH];
  int32_t rssi;
};

struct AppSettings {
  String apSsid;
  String stationSsid;
  String stationPassword;
  String displayMode;
  uint32_t wifiIntervalSeconds;
  uint32_t bleIntervalSeconds;
  uint32_t bleDurationSeconds;
  uint32_t pageIntervalMs;
  uint8_t contrast;
};

U8G2_SSD1306_72X40_ER_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
WebServer server(80);
Preferences preferences;
BLEScan* bleScanner = nullptr;
SemaphoreHandle_t bleStateMutex = nullptr;

AppSettings settings;
WifiNetwork wifiNetworks[MAX_WIFI_NETWORKS];
BleDevice bleDevices[MAX_BLE_DEVICES];
uint8_t wifiNetworkCount = 0;
uint8_t bleDeviceCount = 0;

bool wifiScanRunning = false;
bool bleScanRunning = false;
bool wifiScanFailed = false;
uint32_t lastWifiScanAt = 0;
uint32_t lastBleScanAt = 0;
uint32_t lastDisplayRefreshAt = 0;
uint32_t lastModeChangeAt = 0;
uint32_t activeBleScanDurationSeconds = DEFAULT_BLE_DURATION_SECONDS;
uint8_t cycleMode = 0;
int gazeX = 0;
int gazeY = 0;
uint32_t nextGazeAt = 0;
uint32_t nextBlinkAt = 0;
uint32_t blinkStartedAt = 0;
bool blinking = false;

const char* const DISPLAY_MODES[] = {"wifi", "ble", "assistant", "cycle"};
constexpr uint8_t DISPLAY_MODE_COUNT =
    sizeof(DISPLAY_MODES) / sizeof(DISPLAY_MODES[0]);

String jsonEscape(const String& input) {
  String output;
  output.reserve(input.length() + 8);
  for (size_t i = 0; i < input.length(); ++i) {
    const char character = input[i];
    switch (character) {
      case '"':
        output += F("\\\"");
        break;
      case '\\':
        output += F("\\\\");
        break;
      case '\n':
        output += F("\\n");
        break;
      case '\r':
        output += F("\\r");
        break;
      case '\t':
        output += F("\\t");
        break;
      default:
        if (static_cast<uint8_t>(character) >= 0x20) output += character;
        break;
    }
  }
  return output;
}

bool isValidDisplayMode(const String& mode) {
  for (uint8_t i = 0; i < DISPLAY_MODE_COUNT; ++i) {
    if (mode == DISPLAY_MODES[i]) return true;
  }
  return false;
}

String makeDefaultAccessPointName() {
  const uint64_t chipId = ESP.getEfuseMac();
  char name[32];
  snprintf(name, sizeof(name), "FieldHub-%04X",
           static_cast<uint16_t>(chipId & 0xFFFF));
  return String(name);
}

void loadSettings() {
  preferences.begin("field-hub", false);
  settings.apSsid = preferences.getString("ap-ssid", makeDefaultAccessPointName());
  preferences.remove("web-pass");
  preferences.remove("ap-pass");
  settings.stationSsid = preferences.getString("sta-ssid", "");
  settings.stationPassword = preferences.getString("sta-pass", "");
  settings.displayMode = preferences.getString("mode", "cycle");
  if (!isValidDisplayMode(settings.displayMode)) settings.displayMode = "cycle";
  settings.wifiIntervalSeconds =
      preferences.getUInt("wifi-interval", DEFAULT_WIFI_INTERVAL_SECONDS);
  settings.bleIntervalSeconds =
      preferences.getUInt("ble-interval", DEFAULT_BLE_INTERVAL_SECONDS);
  settings.bleDurationSeconds =
      preferences.getUInt("ble-duration", DEFAULT_BLE_DURATION_SECONDS);
  settings.pageIntervalMs =
      preferences.getUInt("page-interval", DEFAULT_PAGE_INTERVAL_MS);
  settings.contrast = preferences.getUChar("contrast", 180);
}

void startNetwork() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  if (!WiFi.softAP(settings.apSsid.c_str())) {
    Serial.println(F("ERROR: Could not start the Wi-Fi access point."));
  }

  if (!settings.stationSsid.isEmpty()) {
    WiFi.begin(settings.stationSsid.c_str(), settings.stationPassword.c_str());
    Serial.printf("Connecting to router network: %s\n",
                  settings.stationSsid.c_str());
  }
}

void startWifiScan() {
  if (wifiScanRunning || bleScanRunning) return;
  WiFi.scanDelete();
  wifiNetworkCount = 0;
  wifiScanFailed = false;
  wifiScanRunning = true;
  lastWifiScanAt = millis();
  const int result = WiFi.scanNetworks(true, false);
  if (result == WIFI_SCAN_FAILED) {
    wifiScanRunning = false;
    wifiScanFailed = true;
    Serial.println(F("Wi-Fi scan could not be started."));
  }
}

void serviceWifiScan() {
  if (!wifiScanRunning) return;
  const int result = WiFi.scanComplete();
  if (result == WIFI_SCAN_RUNNING) return;

  wifiScanRunning = false;
  if (result < 0) {
    wifiScanFailed = true;
    Serial.printf("Wi-Fi scan failed: %d\n", result);
    WiFi.scanDelete();
    return;
  }

  wifiNetworkCount = min(result, static_cast<int>(MAX_WIFI_NETWORKS));
  for (uint8_t i = 0; i < wifiNetworkCount; ++i) {
    wifiNetworks[i].ssid = WiFi.SSID(i);
    wifiNetworks[i].rssi = WiFi.RSSI(i);
    wifiNetworks[i].channel = WiFi.channel(i);
    wifiNetworks[i].secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
  }
  Serial.printf("Wi-Fi scan complete: %u networks\n", wifiNetworkCount);
  WiFi.scanDelete();
}

void bleScanTask(void*) {
  BleDevice results[MAX_BLE_DEVICES] = {};
  uint8_t resultCount = 0;

  BLEScanResults* scanResults =
      bleScanner->start(activeBleScanDurationSeconds, false);
  if (scanResults != nullptr) {
    const int count = min(scanResults->getCount(),
                          static_cast<int>(MAX_BLE_DEVICES));
    for (int i = 0; i < count; ++i) {
      BLEAdvertisedDevice device = scanResults->getDevice(i);
      String name = device.haveName()
                        ? device.getName()
                        : device.getAddress().toString();
      name.toCharArray(results[resultCount].name,
                       sizeof(results[resultCount].name));
      const String address = device.getAddress().toString();
      address.toCharArray(results[resultCount].address,
                          sizeof(results[resultCount].address));
      results[resultCount].rssi = device.getRSSI();
      ++resultCount;
    }
  }
  bleScanner->clearResults();

  xSemaphoreTake(bleStateMutex, portMAX_DELAY);
  memcpy(bleDevices, results, sizeof(results));
  bleDeviceCount = resultCount;
  bleScanRunning = false;
  lastBleScanAt = millis();
  xSemaphoreGive(bleStateMutex);

  Serial.printf("BLE scan complete: %u devices\n", resultCount);
  vTaskDelete(nullptr);
}

bool startBleScan() {
  xSemaphoreTake(bleStateMutex, portMAX_DELAY);
  if (bleScanRunning || wifiScanRunning) {
    xSemaphoreGive(bleStateMutex);
    return false;
  }
  bleScanRunning = true;
  bleDeviceCount = 0;
  activeBleScanDurationSeconds = settings.bleDurationSeconds;
  xSemaphoreGive(bleStateMutex);

  const BaseType_t taskStarted =
      xTaskCreate(bleScanTask, "ble-scan", 8192, nullptr, 1, nullptr);
  if (taskStarted != pdPASS) {
    xSemaphoreTake(bleStateMutex, portMAX_DELAY);
    bleScanRunning = false;
    lastBleScanAt = millis();
    xSemaphoreGive(bleStateMutex);
    return false;
  }
  return true;
}

void sendConflict(const char* message) {
  server.send(409, "application/json",
              String(F("{\"error\":\"")) + jsonEscape(message) + F("\"}"));
}

void handleStatus() {
  xSemaphoreTake(bleStateMutex, portMAX_DELAY);
  const uint8_t currentBleCount = bleDeviceCount;
  const bool currentBleScanRunning = bleScanRunning;
  xSemaphoreGive(bleStateMutex);

  String body;
  body.reserve(384);
  body = F("{\"apSsid\":\"");
  body += jsonEscape(settings.apSsid);
  body += F("\",\"apIp\":\"");
  body += WiFi.softAPIP().toString();
  body += F("\",\"stationConnected\":");
  body += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
  body += F(",\"stationSsid\":\"");
  body += jsonEscape(settings.stationSsid);
  body += F("\",\"stationIp\":\"");
  body += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  body += F("\",\"wifiCount\":");
  body += String(wifiNetworkCount);
  body += F(",\"bleCount\":");
  body += String(currentBleCount);
  body += F(",\"wifiScanning\":");
  body += wifiScanRunning ? F("true") : F("false");
  body += F(",\"bleScanning\":");
  body += currentBleScanRunning ? F("true") : F("false");
  body += F(",\"wifiScanFailed\":");
  body += wifiScanFailed ? F("true") : F("false");
  body += F(",\"mode\":\"");
  body += settings.displayMode;
  body += F("\"}");
  server.send(200, "application/json", body);
}

void handleWifiResults() {
  String body;
  body.reserve(128 + wifiNetworkCount * 96);
  body = F("{\"networks\":[");
  for (uint8_t i = 0; i < wifiNetworkCount; ++i) {
    if (i > 0) body += ',';
    body += F("{\"ssid\":\"");
    body += jsonEscape(wifiNetworks[i].ssid.isEmpty()
                           ? String(F("(hidden network)"))
                           : wifiNetworks[i].ssid);
    body += F("\",\"rssi\":");
    body += String(wifiNetworks[i].rssi);
    body += F(",\"channel\":");
    body += String(wifiNetworks[i].channel);
    body += F(",\"secured\":");
    body += wifiNetworks[i].secured ? F("true") : F("false");
    body += '}';
  }
  body += F("]}");
  server.send(200, "application/json", body);
}

void handleBleResults() {
  xSemaphoreTake(bleStateMutex, portMAX_DELAY);
  String body;
  body.reserve(128 + bleDeviceCount * 112);
  body = F("{\"devices\":[");
  for (uint8_t i = 0; i < bleDeviceCount; ++i) {
    if (i > 0) body += ',';
    body += F("{\"name\":\"");
    body += jsonEscape(String(bleDevices[i].name));
    body += F("\",\"address\":\"");
    body += jsonEscape(String(bleDevices[i].address));
    body += F("\",\"rssi\":");
    body += String(bleDevices[i].rssi);
    body += '}';
  }
  body += F("]}");
  xSemaphoreGive(bleStateMutex);
  server.send(200, "application/json", body);
}

void handleSettings() {
  if (server.method() == HTTP_GET) {
    String body;
    body.reserve(320);
    body = F("{\"apSsid\":\"");
    body += jsonEscape(settings.apSsid);
    body += F("\",\"stationSsid\":\"");
    body += jsonEscape(settings.stationSsid);
    body += F("\",\"mode\":\"");
    body += settings.displayMode;
    body += F("\",\"wifiInterval\":");
    body += String(settings.wifiIntervalSeconds);
    body += F(",\"bleInterval\":");
    body += String(settings.bleIntervalSeconds);
    body += F(",\"bleDuration\":");
    body += String(settings.bleDurationSeconds);
    body += F(",\"pageInterval\":");
    body += String(settings.pageIntervalMs);
    body += F(",\"contrast\":");
    body += String(settings.contrast);
    body += F("}");
    server.send(200, "application/json", body);
    return;
  }

  const String mode = server.arg("mode");
  const String wifiInterval = server.arg("wifiInterval");
  const String bleInterval = server.arg("bleInterval");
  const String bleDuration = server.arg("bleDuration");
  const String pageInterval = server.arg("pageInterval");
  const String contrast = server.arg("contrast");
  const String apSsid = server.arg("apSsid");
  const String stationSsid = server.arg("stationSsid");
  const String stationPassword = server.arg("stationPassword");

  const long wifiIntervalValue = wifiInterval.toInt();
  const long bleIntervalValue = bleInterval.toInt();
  const long bleDurationValue = bleDuration.toInt();
  const long pageIntervalValue = pageInterval.toInt();
  const long contrastValue = contrast.toInt();
  if (!isValidDisplayMode(mode) || wifiIntervalValue < 10 ||
      wifiIntervalValue > 600 || bleIntervalValue < 15 ||
      bleIntervalValue > 600 || bleDurationValue < 2 ||
      bleDurationValue > 10 || pageIntervalValue < 1200 ||
      pageIntervalValue > 15000 || contrastValue < 0 ||
      contrastValue > 255 || apSsid.isEmpty() || apSsid.length() > 31 ||
      stationSsid.length() > 32 || stationPassword.length() > 63 ||
      (!stationSsid.isEmpty() && stationPassword.length() > 0 &&
       stationPassword.length() < 8)) {
    server.send(400, "application/json",
                F("{\"error\":\"Invalid settings. Check the allowed ranges and password lengths.\"}"));
    return;
  }

  String nextStationPassword = stationPassword;
  if (stationSsid.isEmpty()) {
    nextStationPassword = "";
  } else if (stationPassword.isEmpty() &&
             stationSsid == settings.stationSsid) {
    nextStationPassword = settings.stationPassword;
  }

  const bool accessPointChanged = settings.apSsid != apSsid;
  const bool stationChanged =
      settings.stationSsid != stationSsid ||
      settings.stationPassword != nextStationPassword;

  settings.displayMode = mode;
  settings.wifiIntervalSeconds = wifiIntervalValue;
  settings.bleIntervalSeconds = bleIntervalValue;
  settings.bleDurationSeconds = bleDurationValue;
  settings.pageIntervalMs = pageIntervalValue;
  settings.contrast = contrastValue;
  settings.apSsid = apSsid;
  settings.stationSsid = stationSsid;
  settings.stationPassword = nextStationPassword;

  preferences.putString("mode", settings.displayMode);
  preferences.putUInt("wifi-interval", settings.wifiIntervalSeconds);
  preferences.putUInt("ble-interval", settings.bleIntervalSeconds);
  preferences.putUInt("ble-duration", settings.bleDurationSeconds);
  preferences.putUInt("page-interval", settings.pageIntervalMs);
  preferences.putUChar("contrast", settings.contrast);
  preferences.putString("ap-ssid", settings.apSsid);
  preferences.putString("sta-ssid", settings.stationSsid);
  preferences.putString("sta-pass", settings.stationPassword);
  display.setContrast(settings.contrast);

  if (accessPointChanged) {
    WiFi.softAPdisconnect(true);
    startNetwork();
  } else if (stationChanged) {
    WiFi.disconnect(false, false);
    if (!settings.stationSsid.isEmpty()) {
      WiFi.begin(settings.stationSsid.c_str(), settings.stationPassword.c_str());
    }
  }
  String response = F("{\"saved\":true,\"networkRestarted\":");
  response += accessPointChanged || stationChanged ? F("true") : F("false");
  response += '}';
  server.send(200, "application/json", response);
}

void handleWifiScanRequest() {
  if (bleScanRunning || wifiScanRunning) {
    sendConflict("A radio scan is already running.");
    return;
  }
  startWifiScan();
  if (wifiScanFailed) {
    server.send(503, "application/json",
                F("{\"error\":\"Could not start the Wi-Fi scan.\"}"));
    return;
  }
  server.send(202, "application/json", F("{\"started\":true}"));
}

void handleBleScanRequest() {
  if (!startBleScan()) {
    sendConflict("A radio scan is already running, or the BLE task could not start.");
    return;
  }
  server.send(202, "application/json", F("{\"started\":true}"));
}

void configureWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", WEB_UI);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/wifi", HTTP_GET, handleWifiResults);
  server.on("/api/ble", HTTP_GET, handleBleResults);
  server.on("/api/settings", HTTP_GET, handleSettings);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/wifi/scan", HTTP_POST, handleWifiScanRequest);
  server.on("/api/ble/scan", HTTP_POST, handleBleScanRequest);
  server.onNotFound([]() {
    server.send(404, "application/json", F("{\"error\":\"Not found\"}"));
  });
  server.begin();
}

void drawSignal(int x, int baseline, int rssi) {
  int bars = rssi >= -50 ? 4 : rssi >= -60 ? 3 : rssi >= -70 ? 2 : rssi >= -85 ? 1 : 0;
  for (int i = 0; i < 4; ++i) {
    const int height = (i + 1) * 2;
    if (i < bars) {
      display.drawBox(x + i * 3, baseline - height, 2, height);
    } else {
      display.drawFrame(x + i * 3, baseline - height, 2, height);
    }
  }
}

String shortLabel(const String& source, uint8_t limit) {
  if (source.length() <= limit) return source;
  return source.substring(0, limit - 1) + '~';
}

void drawAssistant(uint32_t now) {
  if (now >= nextGazeAt) {
    const int8_t gazeOptions[][2] = {{0, 0}, {-4, 0}, {4, 0}, {0, -3}, {-3, 2}, {3, 2}};
    const uint8_t index = random(0, 6);
    gazeX = gazeOptions[index][0];
    gazeY = gazeOptions[index][1];
    nextGazeAt = now + random(1000, 2600);
  }
  if (!blinking && now >= nextBlinkAt) {
    blinking = true;
    blinkStartedAt = now;
  }
  float blink = 0.0f;
  if (blinking) {
    const float progress = static_cast<float>(now - blinkStartedAt) / 160.0f;
    if (progress >= 1.0f) {
      blinking = false;
      nextBlinkAt = now + random(2200, 5000);
    } else {
      blink = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
    }
  }

  const int eyeHeight = 26 - static_cast<int>(23 * blink);
  const int eyeTop = (SCREEN_HEIGHT - eyeHeight) / 2;
  const int eyeRadius = min(7, eyeHeight / 2);
  const int eyePositions[] = {4, 40};
  for (uint8_t i = 0; i < 2; ++i) {
    display.setDrawColor(1);
    display.drawRBox(eyePositions[i], eyeTop, 28, eyeHeight, eyeRadius);
    if (eyeHeight > 12) {
      display.setDrawColor(0);
      display.drawDisc(eyePositions[i] + 14 + gazeX,
                       SCREEN_HEIGHT / 2 + gazeY, 5);
      display.setDrawColor(1);
      display.drawDisc(eyePositions[i] + 12 + gazeX,
                       SCREEN_HEIGHT / 2 - 2 + gazeY, 1);
    }
  }
}

void drawScannerScreen(const char* title, bool isBle) {
  if (isBle) xSemaphoreTake(bleStateMutex, portMAX_DELAY);
  display.setFont(u8g2_font_5x7_tf);
  display.drawStr(0, 7, title);
  display.drawHLine(0, 9, SCREEN_WIDTH);
  if (isBle ? bleScanRunning : wifiScanRunning) {
    display.drawStr(2, 25, "Scanning...");
    if (isBle) xSemaphoreGive(bleStateMutex);
    return;
  }

  const uint8_t count = isBle ? bleDeviceCount : wifiNetworkCount;
  if (count == 0) {
    display.drawStr(2, 24, isBle ? "No BLE devices" : "No networks");
    display.drawHLine(0, 32, SCREEN_WIDTH);
    display.setCursor(0, 39);
    display.print(isBle ? "BLE READY" : "WIFI READY");
    if (isBle) xSemaphoreGive(bleStateMutex);
    return;
  }

  const uint8_t rows = isBle ? 2 : 2;
  const uint8_t pageCount = (count + rows - 1) / rows;
  const uint8_t page = (millis() / settings.pageIntervalMs) % pageCount;
  const uint8_t first = page * rows;
  for (uint8_t row = 0; row < rows && first + row < count; ++row) {
    const uint8_t i = first + row;
    const int y = 19 + row * 10;
    const String label = isBle
                             ? shortLabel(String(bleDevices[i].name), 7)
                             : shortLabel(wifiNetworks[i].ssid.isEmpty()
                                              ? String("(hidden)")
                                              : wifiNetworks[i].ssid,
                                          7);
    display.setCursor(0, y);
    display.print(label);
    display.setCursor(37, y);
    display.print(isBle ? bleDevices[i].rssi : wifiNetworks[i].rssi);
    drawSignal(60, y, isBle ? bleDevices[i].rssi : wifiNetworks[i].rssi);
  }
  display.drawHLine(0, 32, SCREEN_WIDTH);
  display.setCursor(0, 39);
  display.print(count);
  display.print(isBle ? " BLE" : " NET");
  display.setCursor(48, 39);
  display.print(page + 1);
  display.print('/');
  display.print(pageCount);
  if (isBle) xSemaphoreGive(bleStateMutex);
}

void drawDisplay(uint32_t now) {
  if (settings.displayMode == "cycle" &&
      now - lastModeChangeAt >= settings.pageIntervalMs) {
    cycleMode = (cycleMode + 1) % 3;
    lastModeChangeAt = now;
  }
  String mode = settings.displayMode;
  if (mode == "cycle") {
    mode = cycleMode == 0 ? "wifi" : cycleMode == 1 ? "ble" : "assistant";
  }

  display.clearBuffer();
  if (mode == "assistant") {
    drawAssistant(now);
  } else if (mode == "ble") {
    drawScannerScreen("BLE SCANNER", true);
  } else {
    drawScannerScreen("WIFI SCANNER", false);
  }
  display.sendBuffer();
}

void serviceAutomaticScans(uint32_t now) {
  if (!wifiScanRunning && !bleScanRunning &&
      now - lastWifiScanAt >= settings.wifiIntervalSeconds * 1000UL) {
    startWifiScan();
  }
  if (!wifiScanRunning && !bleScanRunning &&
      now - lastBleScanAt >= settings.bleIntervalSeconds * 1000UL) {
    startBleScan();
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(150);
  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin();
  loadSettings();
  display.setContrast(settings.contrast);
  randomSeed(esp_random());

  bleStateMutex = xSemaphoreCreateMutex();
  if (bleStateMutex == nullptr) {
    Serial.println(F("ERROR: Could not initialize BLE state lock."));
    while (true) delay(1000);
  }

  BLEDevice::init("");
  bleScanner = BLEDevice::getScan();
  bleScanner->setActiveScan(true);
  bleScanner->setInterval(100);
  bleScanner->setWindow(99);

  startNetwork();
  configureWebServer();

  Serial.println();
  Serial.println(F("ESP32 Field Hub ready"));
  Serial.printf("Wi-Fi network: %s\n", settings.apSsid.c_str());
  Serial.println(F("Wi-Fi password: none (open access point)"));
  Serial.printf("Web dashboard: http://%s/\n", WiFi.softAPIP().toString().c_str());
  Serial.println(F("Dashboard authentication: disabled"));

  lastWifiScanAt = millis() - settings.wifiIntervalSeconds * 1000UL;
  lastBleScanAt = millis() - settings.bleIntervalSeconds * 1000UL;
  nextGazeAt = millis() + 1000;
  nextBlinkAt = millis() + random(1800, 3500);
  startWifiScan();
}

void loop() {
  server.handleClient();
  serviceWifiScan();
  serviceAutomaticScans(millis());

  const uint32_t now = millis();
  if (now - lastDisplayRefreshAt >= DISPLAY_REFRESH_MS) {
    lastDisplayRefreshAt = now;
    drawDisplay(now);
  }
  delay(2);
}
