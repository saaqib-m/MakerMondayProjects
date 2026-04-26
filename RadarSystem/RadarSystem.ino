/*
  Arduino Radar System
  Display: 0.96" SSD1306 OLED (128x64, I2C)
  
  Wiring recap:
    Servo signal  → D9
    HC-SR04 Trig  → D10
    HC-SR04 Echo  → D11
    LED (+ 220Ω)  → D8
    OLED SDA      → A4
    OLED SCL      → A5
    OLED VCC      → 3.3V (or 5V — most modules accept both)
    OLED GND      → GND
*/

#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── Pin definitions ──────────────────────────────────────────────
#define SERVO_PIN     9
#define TRIG_PIN      10
#define ECHO_PIN      11
#define LED_PIN       8

// ── Settings ─────────────────────────────────────────────────────
#define ALERT_DISTANCE_CM  30
#define SWEEP_MIN          0
#define SWEEP_MAX          180
#define SWEEP_STEP         5
#define STEP_DELAY_MS      50

// ── OLED setup ───────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1   // no reset pin on most cheap modules
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Servo radarServo;

int  currentAngle = SWEEP_MIN;
bool sweepForward = true;

// ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN,  OUTPUT);

  radarServo.attach(SERVO_PIN, 500, 2500);  // min µs, max µs
  radarServo.write(SWEEP_MIN);
  delay(500);

  // Start OLED (address 0x3C is standard for 0.96" modules)
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found — check wiring");
    while (true);   // halt so you know something is wrong
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 24);
  display.print("Radar starting...");
  display.display();
  delay(1500);
}

// ─────────────────────────────────────────────────────────────────
void loop() {
  radarServo.write(currentAngle);
  delay(STEP_DELAY_MS);

  long distance = measureDistance();
  bool alert    = (distance > 0 && distance < ALERT_DISTANCE_CM);

  digitalWrite(LED_PIN, alert ? HIGH : LOW);

  updateOLED(currentAngle, distance, alert);

  Serial.print("Angle: ");
  Serial.print(currentAngle);
  Serial.print("  Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  // Advance sweep
  if (sweepForward) {
    currentAngle += SWEEP_STEP;
    if (currentAngle >= SWEEP_MAX) { currentAngle = SWEEP_MAX; sweepForward = false; }
  } else {
    currentAngle -= SWEEP_STEP;
    if (currentAngle <= SWEEP_MIN) { currentAngle = SWEEP_MIN; sweepForward = true; }
  }
}

// ─────────────────────────────────────────────────────────────────
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return 0;
  return duration * 0.0343 / 2;
}

// ─────────────────────────────────────────────────────────────────
void updateOLED(int angle, long distance, bool alert) {
  display.clearDisplay();

  // ── Top bar: title ───────────────────────────────────────────
  display.setTextSize(1);
  display.setCursor(35, 0);
  display.print("[ RADAR ]");

  // ── Divider line ─────────────────────────────────────────────
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // ── Angle (large text, left column) ──────────────────────────
  display.setTextSize(1);
  display.setCursor(0, 14);
  display.print("Angle");

  display.setTextSize(2);
  display.setCursor(0, 24);
  display.print(angle);
  display.print("\xF8");   // degree symbol in extended ASCII

  // ── Distance (large text, right column) ──────────────────────
  display.setTextSize(1);
  display.setCursor(70, 14);
  display.print("Distance");

  display.setTextSize(2);
  display.setCursor(70, 24);
  if (distance == 0) {
    display.print("---");
  } else {
    display.print(distance);
    display.setTextSize(1);
    display.setCursor(70, 42);
    display.print("cm");
  }

  // ── Alert banner (bottom) ─────────────────────────────────────
  display.drawLine(0, 50, 127, 50, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 54);
  if (alert) {
    display.print("!! OBJECT DETECTED !!");
  } else {
    display.print("     All clear");
  }

  display.display();   // push buffer to screen
}