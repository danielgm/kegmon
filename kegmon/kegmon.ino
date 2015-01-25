#include "SPI.h"
#include "WiFly.h"
#include "Credentials.h"

int flexiForcePin = A0;

WiFlyClient client("api.xively.com" , 80);

unsigned long startTime;
unsigned long timeoutTime;
String responseBuffer;

#define MAX_REQUEST_RETRIES 3
int requestRetries;

int state;

unsigned long readingSum;
int numReadings;

#define STATE_DISCONNECTED 1
#define STATE_CONNECTED 2
#define STATE_REQUESTED 3
#define STATE_RESPONDED 4

void setup() {
  Serial.begin(9600);
  startTime = millis();
  responseBuffer = "";
  state = STATE_DISCONNECTED;
  requestRetries = 0;

  readingSum = 0;
  numReadings = 0;

  p("Initialized.");
}

void loop() {
  int flexiForceReading = analogRead(flexiForcePin);
  readingSum += flexiForceReading;
  numReadings++;

  switch (state) {
    case STATE_DISCONNECTED:
      if (ready()) {
       if (connect()) {
          p();
          p("STATE_CONNECTED");
          state = STATE_CONNECTED;
        }

        startTime = millis();
      }
      break;

    case STATE_CONNECTED:
      if (ready()) {
        if (request(floor((float) readingSum / numReadings))) {
          readingSum = 0;
          numReadings = 0;

          p("STATE_REQUESTED");
          state = STATE_REQUESTED;
        }
        else if (timedOut()) {
          if (++requestRetries < MAX_REQUEST_RETRIES) {
          }
          else {
            p("STATE_DISCONNECTED");
            state = STATE_DISCONNECTED;
            requestRetries = 0;
            client.stop();
          }
        }

        startTime = millis();
      }
      break;

    case STATE_REQUESTED:
      if (ready()) {
        if (receive()) {
          p("STATE_RESPONDED");
          state = STATE_RESPONDED;
          requestRetries = 0;
          client.stop();
        }
        else if (timedOut()) {
          if (++requestRetries < MAX_REQUEST_RETRIES) {
            p("STATE_CONNECTED");
            state = STATE_CONNECTED;
          }
          else {
            p("STATE_DISCONNECTED");
            state = STATE_DISCONNECTED;
            requestRetries = 0;
            client.stop();
          }
        }

        startTime = millis();
      }
      break;

    case STATE_RESPONDED:
      if (ready()) {
        p("STATE_DISCONNECTED");
        state = STATE_DISCONNECTED;

        startTime = millis();
      }
      break;

    default:
      p("Error: unknown state.");
  }

  delay(250);
}

bool ready() {
  int delay;
  switch (state) {
    case STATE_DISCONNECTED:
      // Delay before attempting to connect.
      delay = 5000;
      break;

    case STATE_CONNECTED:
      // Delay before making a request.
      delay = 5000;
      break;

    case STATE_REQUESTED:
      // Delay between checking for response.
      delay = 500;
      break;

    case STATE_RESPONDED:
      // Delay before moving back to disconnected state.
      delay = 300000;
      break;
  }

  if (millis() > startTime + delay) {
    return true;
  }
  else {
    Serial.print(".");
    return false;
  }
}

bool timedOut() {
  return millis() > timeoutTime;
}

bool connect() {
  p();
  p("Wifly trying to join.");

  WiFly.begin();
  if (WiFly.join(ssid, passphrase)) {
    p("Joined '" + String(ssid) + "'");
    return true;
  }
  else {
    p("Failed to join network. Waiting to try again.");
    return false;
  }
}

bool request(int reading) {
  if (client.connected() || client.connect()) {
    p("Sending request. v=" + String(reading));
    p();

    // Weird bug if you don't initialize it as a regular string first, then start adding values.
    // @see http://arduino.cc/en/Tutorial/StringAdditionOperator#.Uyqhbq1dUdI
    String data = "{\"version\":\"1.0.0\",\"datastreams\": [{\"id\":\"forceSensor\",\"current_value\":\"";
    data = data + reading + "\"}]}";

    clientPrintln("PUT /v2/feeds/734040001.json HTTP/1.1");
    clientPrintln("X-ApiKey: " + xivelyApiKey);
    clientPrintln("User-Agent: Arduino/1.0");
    clientPrintln("Host: api.xively.com");
    clientPrintln("Content-Type: text/json");
    clientPrintln("Content-Length: " + String(data.length()));
    clientPrintln("Connection: close");
    clientPrintln();
    clientPrintln(data);
    clientPrintln();
    clientPrintln();

    p("Request sent.");
    timeoutTime = startTime + 5000;
    return true;
  }
  else {
    p("Error connecting with remote host");
    return false;
  }
}

bool receive() {
  char c;
  bool gotHttp200 = false;

  p();
  p("--- BEGIN Receive Chunk");
  while (client.available()) {
    char c = client.read();
    if (c == '\n' || c == '\r') {
      if (responseBuffer != "") {
        p("> " + responseBuffer);
        gotHttp200 = gotHttp200 || responseBuffer == "HTTP/1.1 200 OK";
        responseBuffer = "";
      }
    }
    else {
      responseBuffer.concat(c);
    }
  }
  p("--- END Receive Chunk");

  if (gotHttp200) {
    p("Got HTTP 200!");
  }

  responseBuffer = "";
  return gotHttp200;
}

void p(String str) {
  Serial.println(str);
}

void p() {
  Serial.println();
}

void clientPrintln(String str) {
  client.println(str);
  Serial.println(str);
}

void clientPrintln() {
  client.println();
  Serial.println();
}
