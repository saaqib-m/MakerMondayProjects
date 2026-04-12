#include <LedControl.h>

LedControl lc = LedControl(11, 13, 10, 1);

#define BTN_PIN    2
#define BIRD_COL   1
#define PIPE_GAP   5
#define TICK_MS    300

int  birdRow;
int  birdVel;
int  pipeGapTop[8];   // -1 = no pipe in that column
bool gameOver;
bool started;
bool lastBtn;

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);
  lc.shutdown(0, false);
  lc.setIntensity(0, 8);
  lc.clearDisplay(0);
  resetGame();
}

void resetGame() {
  birdRow = 3;
  birdVel = 0;
  gameOver = false;
  started  = false;
  lastBtn  = HIGH;

  // Clear all pipes
  for (int c = 0; c < 8; c++) pipeGapTop[c] = -1;

  // First pipe starts safely far away (column 7)
  pipeGapTop[7] = 2;

  drawFrame();
}

void loop() {
  bool btn = digitalRead(BTN_PIN);
  bool pressed = (btn == LOW && lastBtn == HIGH);
  lastBtn = btn;
  delay(20);  // debounce

  if (gameOver) {
    if (pressed) resetGame();
    return;
  }

  if (!started) {
    // Blink the bird waiting for button
    static bool blink = false;
    blink = !blink;
    lc.clearDisplay(0);
    if (blink) lc.setLed(0, birdRow, BIRD_COL, true);
    if (pressed) started = true;
    delay(TICK_MS);
    return;
  }

  // Apply button
  if (pressed) birdVel = -2;

  // Gravity
  birdVel++;
  if (birdVel > 2) birdVel = 2;
  birdRow += birdVel;

  // Scroll pipes
  for (int c = 0; c < 7; c++) pipeGapTop[c] = pipeGapTop[c + 1];

  // Spawn new pipe on right — ensure gap never spawns at bird column yet
  // Alternate between spawning a pipe and an empty column
  static int spawnCount = 0;
  spawnCount++;
  if (spawnCount % 5 == 0) {
    pipeGapTop[7] = random(0, 8 - PIPE_GAP);
  } else {
    pipeGapTop[7] = -1;
  }

  // Collision check
  if (birdRow < 0 || birdRow > 7) {
    triggerDeath();
    return;
  }
  int gap = pipeGapTop[BIRD_COL];
  if (gap != -1) {
    bool safe = (birdRow >= gap && birdRow < gap + PIPE_GAP);
    if (!safe) {
      triggerDeath();
      return;
    }
  }

  drawFrame();
  delay(TICK_MS);
}

void drawFrame() {
  lc.clearDisplay(0);

  // Draw pipes
  for (int c = 0; c < 8; c++) {
    if (pipeGapTop[c] == -1) continue;
    for (int r = 0; r < 8; r++) {
      bool inGap = (r >= pipeGapTop[c] && r < pipeGapTop[c] + PIPE_GAP);
      if (!inGap) lc.setLed(0, r, c, true);
    }
  }

  // Draw bird
  if (birdRow >= 0 && birdRow <= 7)
    lc.setLed(0, birdRow, BIRD_COL, true);
}

void triggerDeath() {
  gameOver = true;
  for (int i = 0; i < 4; i++) {
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 8; c++)
        lc.setLed(0, r, c, true);
    delay(150);
    lc.clearDisplay(0);
    delay(150);
  }
}