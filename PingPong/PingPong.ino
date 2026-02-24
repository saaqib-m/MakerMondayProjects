#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define P1_BTN 2
#define P2_BTN 3

// How many pixels from the wall counts as "in range" for a deflect
#define DEFLECT_ZONE 10

float ballX, ballY;
float ballVX, ballVY;

int score1 = 0, score2 = 0;

// Grace timer — how many frames after ball enters zone the button still counts
#define GRACE_FRAMES 8
int p1Grace = 0;
int p2Grace = 0;

// Track if deflect already happened this approach
bool p1Deflected = false;
bool p2Deflected = false;

void resetBall(int direction) {
  ballX = SCREEN_WIDTH / 2;
  ballY = random(10, SCREEN_HEIGHT - 10);
  ballVX = direction * 2.5;
  ballVY = (random(2) ? 1.5 : -1.5);
  p1Grace = 0;
  p2Grace = 0;
  p1Deflected = false;
  p2Deflected = false;
}

void showScore() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);

  String s = String(score1) + "-" + String(score2);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT - h) / 2);
  display.print(s);

  display.display();
  delay(1200);
}

void setup() {
  pinMode(P1_BTN, INPUT_PULLUP);
  pinMode(P2_BTN, INPUT_PULLUP);
  randomSeed(analogRead(A0));
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  resetBall(1);
}

void loop() {
  bool p1Hold = !digitalRead(P1_BTN);
  bool p2Hold = !digitalRead(P2_BTN);

  // Move ball
  ballX += ballVX;
  ballY += ballVY;

  // Top / bottom bounce
  if (ballY <= 1) { ballY = 1; ballVY = abs(ballVY); }
  if (ballY >= SCREEN_HEIGHT - 2) { ballY = SCREEN_HEIGHT - 2; ballVY = -abs(ballVY); }

  // --- P1 LEFT WALL ---
  if (ballVX < 0 && ballX <= DEFLECT_ZONE) {
    // Ball is in the deflect zone — start grace timer
    if (p1Grace == 0 && !p1Deflected) p1Grace = GRACE_FRAMES;

    // Button pressed during grace window = deflect
    if (p1Hold && p1Grace > 0 && !p1Deflected) {
      ballVX = abs(ballVX) * 1.1;
      ballVY = ballVY * 1.05 + random(-10, 10) * 0.15;
      ballX = DEFLECT_ZONE;
      p1Deflected = true;
      p1Grace = 0;
    }

    // Ball actually hit the wall without deflect
    if (ballX <= 2 && !p1Deflected) {
      score2++;
      showScore();
      resetBall(1);
      return;
    }
  } else {
    // Ball moving away — reset for next approach
    if (ballVX > 0) { p1Deflected = false; p1Grace = 0; }
  }

  // Count down grace
  if (p1Grace > 0) p1Grace--;
  // If grace expired without deflect and ball is at wall, score
  if (p1Grace == 0 && !p1Deflected && ballX <= 2 && ballVX < 0) {
    score2++;
    showScore();
    resetBall(1);
    return;
  }

  // --- P2 RIGHT WALL ---
  if (ballVX > 0 && ballX >= SCREEN_WIDTH - DEFLECT_ZONE) {
    if (p2Grace == 0 && !p2Deflected) p2Grace = GRACE_FRAMES;

    if (p2Hold && p2Grace > 0 && !p2Deflected) {
      ballVX = -abs(ballVX) * 1.1;
      ballVY = ballVY * 1.05 + random(-10, 10) * 0.15;
      ballX = SCREEN_WIDTH - DEFLECT_ZONE;
      p2Deflected = true;
      p2Grace = 0;
    }

    if (ballX >= SCREEN_WIDTH - 2 && !p2Deflected) {
      score1++;
      showScore();
      resetBall(-1);
      return;
    }
  } else {
    if (ballVX < 0) { p2Deflected = false; p2Grace = 0; }
  }

  if (p2Grace > 0) p2Grace--;
  if (p2Grace == 0 && !p2Deflected && ballX >= SCREEN_WIDTH - 2 && ballVX > 0) {
    score1++;
    showScore();
    resetBall(-1);
    return;
  }

  // Cap speed
  ballVX = constrain(ballVX, -7.0, 7.0);
  ballVY = constrain(ballVY, -5.0, 5.0);

  // --- Draw ---
  display.clearDisplay();

  // Left wall — brightens when ball is in zone
  bool p1Zone = (ballVX < 0 && ballX <= DEFLECT_ZONE);
  if (p1Hold && p1Zone) {
    display.fillRect(0, 0, 3, SCREEN_HEIGHT, WHITE); // solid = ready
  } else if (p1Zone) {
    display.drawLine(0, 0, 0, SCREEN_HEIGHT - 1, WHITE); // thin = incoming!
    display.drawLine(1, 0, 1, SCREEN_HEIGHT - 1, WHITE);
  } else {
    display.drawLine(0, 0, 0, SCREEN_HEIGHT - 1, WHITE);
  }

  // Right wall
  bool p2Zone = (ballVX > 0 && ballX >= SCREEN_WIDTH - DEFLECT_ZONE);
  if (p2Hold && p2Zone) {
    display.fillRect(SCREEN_WIDTH - 3, 0, 3, SCREEN_HEIGHT, WHITE);
  } else if (p2Zone) {
    display.drawLine(SCREEN_WIDTH - 1, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, WHITE);
    display.drawLine(SCREEN_WIDTH - 2, 0, SCREEN_WIDTH - 2, SCREEN_HEIGHT - 1, WHITE);
  } else {
    display.drawLine(SCREEN_WIDTH - 1, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1, WHITE);
  }

  // Dotted center line
  for (int y = 0; y < SCREEN_HEIGHT; y += 4)
    display.drawPixel(SCREEN_WIDTH / 2, y, WHITE);

  // Scores
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(46, 2);
  display.print(score1);
  display.setCursor(72, 2);
  display.print(score2);

  // Ball
  display.fillRect((int)ballX - 1, (int)ballY - 1, 3, 3, WHITE);

  display.display();
  delay(16);
}