#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

// ── Pin definitions ──────────────────────────────────────────
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8

#define PIN_SIGNAL  A0
#define PIN_VGAIN   A1
#define PIN_HSPEED  A2

// ── Display ──────────────────────────────────────────────────
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// ── Screen dimensions (landscape) ────────────────────────────
#define SCREEN_W  320
#define SCREEN_H  240

// ── Axis/plot area margins ────────────────────────────────────
// Leave room on left for Y labels, bottom for X labels
#define MARGIN_LEFT   4
#define MARGIN_BOTTOM 2
#define PLOT_W  (SCREEN_W - MARGIN_LEFT)        // 212px wide plot
#define PLOT_H  (SCREEN_H - MARGIN_BOTTOM)      // 119px tall plot

// ── Colours (RGB565) ─────────────────────────────────────────
#define COL_BG      0x0000   // black
#define COL_GRID    0x0200   // very dark green
#define COL_AXIS    0x07E0   // bright green
#define COL_TRACE   0x07E0   // bright green
#define COL_LABEL   0x4CA0   // muted green for text

// ── Grid divisions ───────────────────────────────────────────
#define GRID_COLS  8
#define GRID_ROWS  4

int prevY = PLOT_H / 2;

// ─────────────────────────────────────────────────────────────
void setup() {
  tft.init(240, 320);
  tft.setRotation(3);          // landscape, left-to-right
  tft.fillScreen(COL_BG);
  drawAxesAndGrid();
  pinMode(3, OUTPUT);
  analogWrite(3, 127);
}

// ─────────────────────────────────────────────────────────────
void loop() {
  static int x = 0;            // x position within plot area

  // Read pots
  int vGainRaw  = analogRead(PIN_VGAIN);
  int hSpeedRaw = analogRead(PIN_HSPEED);

  // Vertical gain: 0.25× to 4×
  float vGain = map(vGainRaw, 0, 1023, 25, 400) / 100.0;

  // Horizontal delay: 0–15 ms between samples
  int hDelay = map(hSpeedRaw, 0, 1023, 0, 15);

  // Read signal, centre on zero, apply gain
  int raw     = analogRead(PIN_SIGNAL);
  int centred = raw - 512;
  int scaled  = (int)(centred * vGain);
  int y = constrain(PLOT_H / 2 - scaled / 8, 0, PLOT_H - 1);

  // Absolute screen coords
  int sx = MARGIN_LEFT + x;
  int sy = y;

  // Clear this column (restore grid/axis underneath)
  tft.drawFastVLine(sx, 0, PLOT_H, COL_BG);
  redrawGridColumn(x);

  // Draw trace segment
  int prevSX = MARGIN_LEFT + (x == 0 ? 0 : x - 1);
  tft.drawLine(prevSX, prevY, sx, sy, COL_TRACE);

  prevY = sy;
  x++;

  // When trace reaches the right edge, clear and redraw
  if (x >= PLOT_W) {
    x = 0;
    tft.fillScreen(COL_BG);
    drawAxesAndGrid();
  }

  if (hDelay > 0) delay(hDelay);
}

// ─────────────────────────────────────────────────────────────
// Draw the full axis frame, grid, tick marks and labels
// ─────────────────────────────────────────────────────────────
void drawAxesAndGrid() {

  // ── Grid lines ───────────────────────────────────────────
  for (int col = 1; col < GRID_COLS; col++) {
    int gx = MARGIN_LEFT + (col * PLOT_W / GRID_COLS);
    for (int py = 0; py < PLOT_H; py += 3) {
      tft.drawPixel(gx, py, COL_GRID);            // dashed vertical
    }
  }
  for (int row = 1; row < GRID_ROWS; row++) {
    int gy = row * PLOT_H / GRID_ROWS;
    for (int px = MARGIN_LEFT; px < SCREEN_W; px += 3) {
      tft.drawPixel(px, gy, COL_GRID);            // dashed horizontal
    }
  }

  // ── Y axis line ──────────────────────────────────────────
  tft.drawFastVLine(MARGIN_LEFT, 0, PLOT_H, COL_AXIS);

  // ── X axis line (centre) ─────────────────────────────────
  tft.drawFastHLine(MARGIN_LEFT, PLOT_H / 2, PLOT_W, COL_AXIS);

  // ── Bottom baseline ──────────────────────────────────────
  tft.drawFastHLine(MARGIN_LEFT, PLOT_H, PLOT_W, COL_AXIS);

  // ── Y axis ticks + voltage labels ────────────────────────
  // Shows +2.5V at top, 0V at centre, -2.5V at bottom (relative to 2.5V mid)
  const char* yLabels[] = {"+V", " 0", "-V"};
  int yPositions[]      = {4, PLOT_H / 2, PLOT_H - 8};

  for (int i = 0; i < 3; i++) {
    int ty = yPositions[i];
    // Tick mark on Y axis
    tft.drawFastHLine(MARGIN_LEFT - 4, ty, 5, COL_AXIS);
    // Label to the left
    tft.setCursor(0, ty - 4);
    tft.setTextColor(COL_LABEL);
    tft.setTextSize(1);
    tft.print(yLabels[i]);
  }

  // ── X axis ticks ─────────────────────────────────────────
  for (int col = 0; col <= GRID_COLS; col++) {
    int tx = MARGIN_LEFT + (col * PLOT_W / GRID_COLS);
    tft.drawFastVLine(tx, PLOT_H, 4, COL_AXIS);
  }

  // ── Corner label ─────────────────────────────────────────
  tft.setCursor(MARGIN_LEFT + 2, PLOT_H + 5);
  tft.setTextColor(COL_LABEL);
  tft.setTextSize(1);
  tft.print("t>");
}

// ─────────────────────────────────────────────────────────────
// Redraw grid pixels on a single plot-area column after clearing
// ─────────────────────────────────────────────────────────────
void redrawGridColumn(int plotX) {
  int sx = MARGIN_LEFT + plotX;

  // Vertical grid line at this column?
  if (plotX % (PLOT_W / GRID_COLS) == 0) {
    for (int py = 0; py < PLOT_H; py += 3) {
      tft.drawPixel(sx, py, COL_GRID);
    }
  }

  // Horizontal grid dots at every row division
  for (int row = 1; row < GRID_ROWS; row++) {
    tft.drawPixel(sx, row * PLOT_H / GRID_ROWS, COL_GRID);
  }

  // Redraw centre X axis and Y axis if needed
  tft.drawPixel(sx, PLOT_H / 2, COL_AXIS);
  if (plotX == 0) {
    tft.drawFastVLine(MARGIN_LEFT, 0, PLOT_H, COL_AXIS);
  }
}