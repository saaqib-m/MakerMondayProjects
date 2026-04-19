#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 10

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

#define MIC_PIN A0
#define COLS 32
#define ROWS 8

float smoothed[COLS];
float targets[COLS];
int targetTimer[COLS];
int peak[COLS];
int decayCounter = 0;
const int DECAY_INTERVAL = 5;

void setup() {
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 3);
  mx.clear();
  Serial.begin(9600);
  randomSeed(analogRead(A1));

  for (int i = 0; i < COLS; i++) {
    smoothed[i] = 0;
    targets[i] = 0;
    peak[i] = 0;
    targetTimer[i] = random(0, 20);
  }
}

void loop() {
  // ===== YOUR WORKING MIC CODE =====
  int raw = analogRead(MIC_PIN);
  int centered = abs(raw - 512);
  Serial.println(centered);
  static float globalSmooth = 0;
  int gated = (centered > 10) ? (centered - 10) : 0;  // Adjust these parameters depending on how much noise you have on the Serial Plotter
  globalSmooth = globalSmooth * 0.85 + gated * 0.15;
  int baseHeight = map((int)globalSmooth, 0, 250, 0, ROWS);  // Adjust how high you want the dots to go up
  baseHeight = constrain(baseHeight, 0, ROWS);

  // ===== UPDATE COLUMN TARGETS =====
  for (int x = 0; x < COLS; x++) {
    targetTimer[x]--;

    if (targetTimer[x] <= 0) {
      int spread = random(-2, 3);
      targets[x] = constrain(baseHeight + spread, 0, ROWS);
      targetTimer[x] = random(5, 20);
    }

    smoothed[x] = smoothed[x] * 0.80 + targets[x] * 0.20;
  }

  // ===== SPATIAL SMOOTH =====
  for (int x = 1; x < COLS - 1; x++) {
    smoothed[x] = smoothed[x] * 0.7 + (smoothed[x-1] + smoothed[x+1]) * 0.15;
  }

  // ===== DRAW =====
  mx.clear();
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  decayCounter++;

  for (int x = 0; x < COLS; x++) {
    int h = constrain((int)round(smoothed[x]), 0, ROWS);

    for (int y = 0; y < h; y++) {
      mx.setPoint(7 - y, x, true);
    }

    if (h >= peak[x]) peak[x] = h;
    if (peak[x] > h) mx.setPoint(7 - peak[x], x, true);
    if (decayCounter >= DECAY_INTERVAL && peak[x] > 0) peak[x]--;
  }

  if (decayCounter >= DECAY_INTERVAL) decayCounter = 0;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
  delay(25);
}