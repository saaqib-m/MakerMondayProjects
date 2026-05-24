#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BUTTON_PIN 2
#define LED_PIN 8

enum State { WAITING, READY, RESULT };
State state = WAITING;

unsigned long ledOnTime = 0;
unsigned long reactionTime = 0;
unsigned long waitDuration = 0;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();

  showMessage("Press button", "to start!");
}

void loop() {
  bool buttonPressed = digitalRead(BUTTON_PIN) == LOW;

  if (state == WAITING && buttonPressed) {
    // Start the random wait
    state = READY;
    waitDuration = random(2000, 6000);  // 2–6 second random delay
    ledOnTime = millis() + waitDuration;
    showMessage("Get ready...", "");
    delay(200);  // debounce
  }

  else if (state == READY) {
    if (millis() >= ledOnTime) {
      // Time to react!
      digitalWrite(LED_PIN, HIGH);
      ledOnTime = millis();  // reuse as start time
      state = RESULT;
      showMessage("NOW!", "Hit the button!");
    }
    // If they press too early
    if (buttonPressed) {
      showMessage("Too early!", "Wait for LED");
      state = WAITING;
      delay(2000);
      showMessage("Press button", "to start!");
    }
  }

  else if (state == RESULT && buttonPressed) {
    reactionTime = millis() - ledOnTime;
    digitalWrite(LED_PIN, LOW);
    state = WAITING;

    // Show result
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Your time:");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(reactionTime);
    display.println(" ms");
    display.setTextSize(1);
    display.setCursor(0, 50);
    if (reactionTime < 200) display.println("Incredible!");
    else if (reactionTime < 300) display.println("Great!");
    else if (reactionTime < 500) display.println("Not bad!");
    else display.println("Keep practising!");
    display.display();

    delay(4000);
    showMessage("Press button", "to try again!");
  }
}

void showMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(line1);
  display.setCursor(0, 36);
  display.println(line2);
  display.display();
}