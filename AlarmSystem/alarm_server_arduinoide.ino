## Install esp32 by Espressif Systems in Board Managers beforehand

#include <WiFi.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const char* host = "YOUR_LAPTOP_IP"; ## 1) Open ipconfig in Terminal; 2) Look for IPv4 Address; 3) Replace here
const int port = 5000;

int trigPin = 5;
int echoPin = 18;
int ledPin = 23;

long duration;
float distance;

float threshold = 100; // cm - set threshold distance

void setup() {

  Serial.begin(115200);

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("Connected!");
}

float readDistance(){

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);

  distance = duration * 0.034 / 2;

  return distance;
}

void sendAlert(){

  WiFiClient client;

  if (!client.connect(host, port)) {
    Serial.println("Connection failed");
    return;
  }

  client.println("MOTION DETECTED");

  client.stop();
}

void loop(){

  float d = readDistance();

  Serial.println(d);

  if(d < threshold){

    digitalWrite(ledPin, HIGH);

    sendAlert();

    delay(5000);
  }
  else{

    digitalWrite(ledPin, LOW);

  }

  delay(200);
}