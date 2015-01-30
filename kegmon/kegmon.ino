#include "SPI.h"
#include "WiFly.h"
#include "Credentials.h"

WiFlyClient client("api.xively.com" , 80);

unsigned long startTime;
unsigned long timeoutTime;

#define RESPONSE_BUFFER_SIZE 128
char responseBuffer[RESPONSE_BUFFER_SIZE];

#define MAX_RETRIES 3
int retries;

int state;

unsigned long readingSum;
int numReadings;

#define STATE_DISCONNECTED 1
#define STATE_CONNECTED 2
#define STATE_REQUESTED 3
#define STATE_TIMED_OUT 4
#define STATE_RESPONDED_NG 5
#define STATE_RESPONDED_OK 6

void setup() {
  Serial.begin(9600);
  startTime = millis();
  responseBuffer[0] = '\0';
  state = STATE_DISCONNECTED;
  retries = 0;

  readingSum = 0;
  numReadings = 0;

  p("Initialized.");
}

void loop() {
  int flexiForceReading = analogRead(A0);
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

          p();
          p("STATE_REQUESTED");
          state = STATE_REQUESTED;
        }
        else if (timedOut()) {
          if (++retries < MAX_RETRIES) {
            p();
            p("STATE_TIMED_OUT");
            state = STATE_TIMED_OUT;
          }
          else {
            p();
            p("STATE_DISCONNECTED (1)");
            state = STATE_DISCONNECTED;
            retries = 0;
            client.stop();
          }
        }

        startTime = millis();
      }
      break;

    case STATE_REQUESTED:
      if (ready()) {
        if (receive()) {
          p();
          p("STATE_RESPONDED_OK");
          state = STATE_RESPONDED_OK;
          retries = 0;
          client.stop();
        }
        else if (timedOut()) {
          if (++retries < MAX_RETRIES) {
            // Could also have been a timeout but not bothering to differentiate.
            p();
            p("STATE_RESPONDED_NG");
            state = STATE_RESPONDED_NG;
          }
          else {
            p();
            p("STATE_DISCONNECTED (2)");
            state = STATE_DISCONNECTED;
            retries = 0;
            client.stop();
          }
        }

        startTime = millis();
      }
      break;

    case STATE_TIMED_OUT:
      if (ready()) {
        p();
        p("STATE_CONNECTED");
        state = STATE_CONNECTED;

        startTime = millis();
      }
      break;

    case STATE_RESPONDED_NG:
      if (ready()) {
        p();
        p("STATE_CONNECTED");
        state = STATE_CONNECTED;

        startTime = millis();
      }
      break;

    case STATE_RESPONDED_OK:
      if (ready()) {
        p();
        p("STATE_DISCONNECTED Ready to reconnect.");
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
  unsigned long waitDuration;
  switch (state) {
    case STATE_DISCONNECTED:
      // Delay before attempting to connect.
      waitDuration = 5000;
      break;

    case STATE_CONNECTED:
      // Delay before making a request.
      waitDuration = 5000;
      break;

    case STATE_REQUESTED:
      // Delay between checking for response.
      waitDuration = 500;
      break;

    case STATE_TIMED_OUT:
      // Delay before retry.
      waitDuration = 30000;
      break;

    case STATE_RESPONDED_NG:
      // Delay before retry.
      waitDuration = 30000;
      break;

    case STATE_RESPONDED_OK:
      // Delay before moving back to disconnected state.
      waitDuration = 240000;
      break;
  }

  if (millis() > startTime + waitDuration) {
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
  p("Wifly trying to join network.");

  WiFly.begin();
  if (WiFly.join(ssid, passphrase)) {
    p("Joined network.");
    return true;
  }
  else {
    p("Failed to join network. Waiting to try again.");
    return false;
  }
}

bool request(int reading) {
  if (client.connected() || client.connect()) {
    p();
    p("Sending request.");
    p(reading);
    p();

    // Arduino's apparently sketchy String class used only to convert int to char[].
    String str;

    char readingStr[5];
    str = String(reading);
    str.toCharArray(readingStr, 5);

    char data[128];
    strcpy(data, "{\"version\":\"1.0.0\",\"datastreams\": [{\"id\":\"forceSensor\",\"current_value\":\"");
    strcat(data, readingStr);
    strcat(data, "\"}]}");

    char dataLenStr[4];
    str = String(strlen(data));
    str.toCharArray(dataLenStr, 4);

    clientPrintln("PUT /v2/feeds/734040001.json HTTP/1.1");
    clientPrint("X-ApiKey: ");
    clientPrintln(xivelyApiKey);
    clientPrintln("User-Agent: Arduino/1.0");
    clientPrintln("Host: api.xively.com");
    clientPrintln("Content-Type: text/json");
    clientPrint("Content-Length: ");
    clientPrintln(dataLenStr);
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
  int i;
  bool gotHttp200 = false;

  p();
  p("--- BEGIN Receive Chunk");
  while (client.available()) {
    char c = client.read();
    if (c == '\n' || c == '\r') {
      if (responseBuffer[0] != '\0') {
        p(responseBuffer);
        gotHttp200 = gotHttp200 || strcmp(responseBuffer, "HTTP/1.1 200 OK");
        responseBuffer[0] = '\0';
      }
    }
    else if (strlen(responseBuffer) < RESPONSE_BUFFER_SIZE) {
      i = strlen(responseBuffer);
      responseBuffer[i] = c;
      responseBuffer[i + 1] = '\0';
    }
  }
  p("--- END Receive Chunk");

  if (gotHttp200) {
    p("Got HTTP 200!");
  }

  responseBuffer[0] = '\0';
  return gotHttp200;
}

void p(String str) {
  Serial.println(str);
}

void p(char* str) {
  Serial.println(str);
}

void p(int str) {
  Serial.println(str);
}

void p() {
  Serial.println();
}

void clientPrint(String str) {
  Serial.print(str);
  client.print(str);
}

void clientPrintln(String str) {
  Serial.println(str);
  client.println(str);
}

void clientPrint(char* str) {
  Serial.print(str);
  client.print(str);
}

void clientPrintln(char* str) {
  Serial.println(str);
  client.println(str);
}

void clientPrint(int x) {
  client.print(x);
  Serial.print(x);
}

void clientPrintln(int x) {
  client.println(x);
  Serial.println(x);
}

void clientPrintln() {
  Serial.println();
  client.println();
}
