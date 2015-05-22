#include <Adafruit_CharacterOLED.h>

/**
 * The circuit:
 * LCD RS pin to digital pin 6
 * LCD R/W pin to digital pin 7
 * LCD Enable pin to digital pin 8
 * LCD D4 pin to digital pin 9
 * LCD D5 pin to digital pin 10
 * LCD D6 pin to digital pin 11
 * LCD D7 pin to digital pin 12
 */
Adafruit_CharacterOLED lcd(OLED_V2, 6, 7, 8, 9, 10, 11, 12);

unsigned long readingStartTime;
unsigned long readingSum;
int numReadings;
int prevValue;

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);

  lcd.setCursor(0, 0);
  lcd.print("Brewfulness");

  readingStartTime = millis();
  readingSum = 0;
  numReadings = 0;
  prevValue = -1;
}

void loop() {
  int flexiForceReading = analogRead(A0);
  readingSum += flexiForceReading;
  numReadings++;

  unsigned long now = millis();
  if (now - readingStartTime > 5000) {
    int value = round((float) readingSum / numReadings);
    if (value != prevValue) {
      lcd.setCursor(0, 1);
      lcd.print("    ");
      lcd.setCursor(0, 1);
      lcd.print(value);

      prevValue = value;
    }

    readingStartTime = now;
    readingSum = 0;
    numReadings = 0;
  }

  delay(100);
}
