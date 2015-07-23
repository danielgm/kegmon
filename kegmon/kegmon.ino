#include "SPI.h"
#include "Ethernet.h"
#include "Credentials.h"


// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0F, 0x06, 0xB6 };
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "api.xively.com";    // name address for Google (using DNS)

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 0, 177);

EthernetClient client;

unsigned long startTime;

#define RESPONSE_BUFFER_SIZE 128
char responseBuffer[RESPONSE_BUFFER_SIZE];

char requestHttpCommand[] = "PUT /v2/feeds/734040001.json HTTP/1.1";
char requestApiKey[] = "X-ApiKey: ";
char requestUserAgent[] = "User-Agent: Arduino/1.0";
char requestHost[] = "Host: api.xively.com";
char requestContentType[] = "Content-Type: text/json";
char requestContentLength[] = "Content-Length: 80";
char requestConnection[] = "Connection: close";
char requestDataPrefix[] = "{\"version\":\"1.0.0\",\"datastreams\": [{\"id\":\"forceSensor\",\"current_value\":\"";
char requestDataSuffix[] = "\"}]}";
char http200[] = "HTTP/1.1 200 OK";

#define MAX_RETRIES 3
int retries;

int state;

unsigned long readingSum;
int numReadings;

#define STATE_DISCONNECTED 1
#define STATE_CONNECTED 2
#define STATE_REQUEST_FAILED 3
#define STATE_REQUESTED 4
#define STATE_TIMED_OUT 5
#define STATE_RESPONDED_NG 6
#define STATE_RESPONDED_OK 7
#define STATE_DISCONNECTED_ERROR 8

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
    case STATE_DISCONNECTED_ERROR:
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
        else if (++retries < MAX_RETRIES) {
          p();
          p("STATE_REQUEST_FAILED");
          p(String(MAX_RETRIES - retries) + " retries remaining.");
          state = STATE_REQUEST_FAILED;
        }
        else {
          p();
          p("STATE_DISCONNECTED (1)");
          state = STATE_DISCONNECTED_ERROR;
          retries = 0;
          client.stop();
        }

        startTime = millis();
      }
      break;

    case STATE_REQUEST_FAILED:
      if (ready()) {
        p();
        p("STATE_CONNECTED");
        state = STATE_CONNECTED;

        startTime = millis();
      }
      break;

    case STATE_REQUESTED:
      if (client.available()) {
        if (receive()) {
          p();
          p("STATE_RESPONDED_OK");
          state = STATE_RESPONDED_OK;
          retries = 0;
          client.stop();
        }
        else if (++retries < MAX_RETRIES) {
          p();
          p("STATE_RESPONDED_NG");
          p(String(MAX_RETRIES - retries) + " retries remaining.");
          state = STATE_RESPONDED_NG;
        }
        else {
          p();
          p("STATE_DISCONNECTED (1)");
          state = STATE_DISCONNECTED_ERROR;
          retries = 0;
          client.stop();
        }

        startTime = millis();
      }
      else if (ready()) {
        if (++retries < MAX_RETRIES) {
          p();
          p("STATE_TIMED_OUT");
          p(String(MAX_RETRIES - retries) + " retries remaining.");
          state = STATE_TIMED_OUT;
        }
        else {
          p();
          p("STATE_DISCONNECTED (2)");
          state = STATE_DISCONNECTED_ERROR;
          retries = 0;
          client.stop();
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
      break;
  }

  delay(250);
}

bool ready() {
  unsigned long waitDuration;
  switch (state) {
    case STATE_DISCONNECTED_ERROR:
      // Delay before attempting to connect after three retries failed.
      waitDuration = 240000;
      break;

    case STATE_DISCONNECTED:
      // Delay before attempting to connect.
      waitDuration = 5000;
      break;

    case STATE_CONNECTED:
      // Delay before making a request.
      waitDuration = 1000;
      break;

    case STATE_REQUEST_FAILED:
      // Delay before retry.
      waitDuration = 30000;
      break;

    case STATE_REQUESTED:
      // Delay before giving up on a response.
      waitDuration = 30000;
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

bool connect() {
  p();
  p("Connecting");
  
  if (Ethernet.begin(mac) == 0) {
    p("Failed to configure Ethernet using DHCP");
    return false;
  }
  
  p("Connected");
  return true;
}

bool request(int reading) {
  if (client.connect(server, 80)) {
    p();
    p("Connected. Sending request.");
    p(reading);
    p();

    // Arduino's apparently sketchy String class used only to convert int to char[].
    String str;

    char readingStr[5];
    str = String(reading);
    str.toCharArray(readingStr, 5);

    clientPrintln(requestHttpCommand);
    clientPrint(requestApiKey);
    clientPrintln(xivelyApiKey);
    clientPrintln(requestUserAgent);
    clientPrintln(requestHost);
    clientPrintln(requestContentType);
    clientPrintln(requestContentLength);
    clientPrintln(requestConnection);
    clientPrintln();
    clientPrint(requestDataPrefix);
    clientPrint(readingStr);
    clientPrint(requestDataSuffix);

    // Pad the request with whitespace to meet the content length.
    if (strlen(readingStr) < 4) clientPrint(" ");
    if (strlen(readingStr) < 3) clientPrint(" ");
    if (strlen(readingStr) < 2) clientPrint(" ");

    clientPrintln();
    clientPrintln();
    clientPrintln();

    p("Request sent.");
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
        gotHttp200 = gotHttp200 || strcmp(responseBuffer, http200);
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
