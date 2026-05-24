void setup() {
  for (int pin = 2; pin <= 9; pin++) pinMode(pin, OUTPUT);
  Serial.begin(9600);
  Serial.println("Enter a number (0-255):");
}

void loop() {
  if (Serial.available()) {
    int num = Serial.parseInt();
    if (num >= 0 && num <= 255) {
      displayBinary(num);

      Serial.print("Decimal: ");
      Serial.print(num);
      Serial.print("   Binary: ");
      for (int bit = 7; bit >= 0; bit--) {
        Serial.print((num >> bit) & 1);
        if (bit == 4) Serial.print(" ");
      }
      Serial.println();

      delay(5000);  // wait 10 seconds
      displayBinary(0);  // turn all LEDs off
      Serial.println("Enter a number (0-255):");
    } else {
      Serial.println("Out of range! Enter 0-255.");
    }
  }
}

void displayBinary(int number) {
  for (int bit = 0; bit < 8; bit++) {
    digitalWrite(2 + bit, (number >> bit) & 1);
  }
}