#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#define DHTPIN 2
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

float temp = 0;
float hum = 0;
float heatIndex = 0;

float maxTemp = -1000;
float minTemp = 1000;
float prevTemp = 0;

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); // Change to 0x3D if needed
  display.clearDisplay();
  display.display();

  dht.begin();
  delay(2000);  // Important for DHT11 stability
}

void loop() {

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temp = t;
    hum = h;
    heatIndex = dht.computeHeatIndex(temp, hum, false);

    if (temp > maxTemp) maxTemp = temp;
    if (temp < minTemp) minTemp = temp;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Temperature + trend
  display.setCursor(0,0);
  display.print("Temp: ");
  display.print(temp,1);
  display.print(" C");

  if (temp > prevTemp) display.print(" ^");
  else if (temp < prevTemp) display.print(" v");

  // Humidity
  display.setCursor(0,12);
  display.print("Hum: ");
  display.print(hum,0);
  display.print(" %");

  // Heat Index
  display.setCursor(0,24);
  display.print("Feels: ");
  display.print(heatIndex,1);
  display.print(" C");

  // Comfort Status
  display.setCursor(0,36);

  if (hum >= 30 && hum <= 60 && temp >= 20 && temp <= 26)
    display.print("Comfort: Good");
  else if (hum > 70)
    display.print("Comfort: Humid");
  else if (hum < 30)
    display.print("Comfort: Dry");
  else if (temp > 30)
    display.print("Comfort: Hot");
  else if (temp < 18)
    display.print("Comfort: Cold");
  else
    display.print("Comfort: OK");

  // Min / Max
  display.setCursor(0,52);
  display.print("Max:");
  display.print(maxTemp,1);

  display.setCursor(64,52);
  display.print("Min:");
  display.print(minTemp,1);

  display.display();

  prevTemp = temp;

  delay(2000);
}