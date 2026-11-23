#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SDA_PIN 5
#define SCL_PIN 6

const int SCREEN_WIDTH = 72;
const int SCREEN_HEIGHT = 40;
const int EYE_WIDTH = 28;
const int EYE_HEIGHT = 26;
const int EYE_LEFT_X = 4;
const int EYE_RIGHT_X = 40;
const int EYE_CENTER_Y = SCREEN_HEIGHT / 2;
const unsigned long GAZE_TRANSITION_MS = 220;
const unsigned long BLINK_DURATION_MS = 150;

struct Gaze {
  int8_t x;
  int8_t y;
};

const Gaze GAZES[] = {
  {0, 0},
  {-4, 0},
  {4, 0},
  {0, -3},
  {-3, 2},
  {3, 2}
};
const int GAZE_COUNT = sizeof(GAZES) / sizeof(GAZES[0]);

U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

int gazeIndex = 0;
int gazeFromX = 0;
int gazeFromY = 0;
unsigned long gazeStartedAt = 0;
unsigned long nextGazeAt = 0;
unsigned long nextBlinkAt = 0;
unsigned long blinkStartedAt = 0;
bool blinking = false;

void updateGaze(unsigned long now, int& gazeX, int& gazeY) {
  if (now >= nextGazeAt) {
    gazeFromX = GAZES[gazeIndex].x;
    gazeFromY = GAZES[gazeIndex].y;
    gazeIndex = (gazeIndex + random(1, GAZE_COUNT)) % GAZE_COUNT;
    gazeStartedAt = now;
    nextGazeAt = now + GAZE_TRANSITION_MS + random(900, 1900);
  }

  float progress = static_cast<float>(now - gazeStartedAt) / GAZE_TRANSITION_MS;
  if (progress > 1.0f) progress = 1.0f;
  const float eased = progress * progress * (3.0f - 2.0f * progress);

  gazeX = gazeFromX + (GAZES[gazeIndex].x - gazeFromX) * eased;
  gazeY = gazeFromY + (GAZES[gazeIndex].y - gazeFromY) * eased;
}

float updateBlink(unsigned long now) {
  if (!blinking && now >= nextBlinkAt) {
    blinking = true;
    blinkStartedAt = now;
  }

  if (!blinking) return 0.0f;

  const float progress = static_cast<float>(now - blinkStartedAt) / BLINK_DURATION_MS;
  if (progress >= 1.0f) {
    blinking = false;
    nextBlinkAt = now + random(2200, 5000);
    return 0.0f;
  }

  return progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
}

void drawEye(int x, int gazeX, int gazeY, float blink) {
  const int height = EYE_HEIGHT - static_cast<int>((EYE_HEIGHT - 3) * blink);
  const int top = EYE_CENTER_Y - height / 2;
  const int radius = min(7, height / 2);

  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, top, EYE_WIDTH, height, radius);

  if (height > 12) {
    const int pupilX = x + EYE_WIDTH / 2 + gazeX;
    const int pupilY = EYE_CENTER_Y + gazeY;
    u8g2.setDrawColor(0);
    u8g2.drawDisc(pupilX, pupilY, 5);
    u8g2.setDrawColor(1);
    u8g2.drawDisc(pupilX - 2, pupilY - 2, 1);
  }
}

void drawAssistantFace(unsigned long now) {
  int gazeX;
  int gazeY;
  updateGaze(now, gazeX, gazeY);
  const float blink = updateBlink(now);

  u8g2.clearBuffer();
  drawEye(EYE_LEFT_X, gazeX, gazeY, blink);
  drawEye(EYE_RIGHT_X, gazeX, gazeY, blink);
  u8g2.sendBuffer();
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();
  randomSeed(micros());

  const unsigned long now = millis();
  gazeStartedAt = now;
  nextGazeAt = now + 1200;
  nextBlinkAt = now + random(1800, 3500);
}

void loop() {
  drawAssistantFace(millis());
  delay(20);
}
