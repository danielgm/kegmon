#include "SPI.h"
#include "WiFly.h"
#include "Credentials.h"

int flexiForcePin = A0;

WiFlyClient client("api.xively.com" , 80);

unsigned long startTime;
bool joined;

void setup() {
  Serial.begin(9600);
  startTime = millis();
  joined = false;
}

void loop() {
  if (joined && millis() > startTime + 5000) {
    if (client.connected() || client.connect()) {
      Serial.println("Got here.");
      int flexiForceReading = analogRead(flexiForcePin);
      Serial.println("Sending request. v=" + String(flexiForceReading));
      Serial.println();
      
      String data = String("{\"version\":\"1.0.0\",\"datastreams\": [{\"id\":\"forceSensor\",\"current_value\":\"")
        + String(flexiForceReading) + String("\"}]}");
      
      Serial.println("PUT /v2/feeds/734040001.json HTTP/1.1");
      Serial.println("X-ApiKey: SU5B4qKyxQ9NjidtFHaiDNpEIjClFCATSqxmnCobzfS5J8kJ");
      Serial.println("User-Agent: Arduino/1.0");
      Serial.println("Host: api.xively.com");
      Serial.println("Content-Type: text/json");
      Serial.println("Content-Length: " + String(data.length()));
      Serial.println("Connection: close");
      Serial.println();
      Serial.println(data);
      client.println();
      client.println();
      
      client.println("PUT /v2/feeds/734040001.json HTTP/1.1");
      client.println("X-ApiKey: SU5B4qKyxQ9NjidtFHaiDNpEIjClFCATSqxmnCobzfS5J8kJ");
      client.println("User-Agent: Arduino/1.0");
      client.println("Host: api.xively.com");
      client.println("Content-Type: text/json");
      client.println("Content-Length: " + String(data.length()));
      client.println("Connection: close");
      client.println();
      client.println(data);
      client.println();
      client.println();
      
      Serial.println("Request sent.");
    }
    else {
      Serial.println("Error connecting with remote host"); 
      joined = false;
    }
    startTime = millis();
  }
  else if (!joined && millis() > startTime + 5000) {
    Serial.println("Wifly trying to join.");
    
    WiFly.begin();
    if (joined = WiFly.join(ssid, passphrase)) {
      Serial.println("Joined '" + String(ssid) + "'");
    }
    else {
      Serial.println("Failed to join network. Waiting to try again.");
    }
    startTime = millis();
  }
  else {
    Serial.print(millis());
    Serial.print(" ");
    Serial.println(startTime);
    delay(1000);
  }
      
  while (client.available()) {
    char c = client.read();
    Serial.print(c);
  }
}
