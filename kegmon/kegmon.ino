#include "SPI.h"
#include "WiFly.h"
#include "Credentials.h"

#include <LiquidCrystal.h>
 
// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
 
int flexiForcePin = A0; //analog pin 0
int high = 200, medium = 100;


char nma_host[]="www.notifymyandroid.com";
String nma_key="insert_your_nma_apikey_here";
 
WiFlyClient client( "beerstatus.herokuapp.com" , 80 );
 
int startTime;
 
bool tryToJoin()
{
  Serial.println("Wifly trying to join.");
  if (!WiFly.join(ssid, passphrase)) 
  {
    return false;
  }  
  else
  {
    return true;
  }
}
 
void setup()
{
  Serial.begin( 9600 ); 
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  
  WiFly.begin();
  while(tryToJoin())
  {
    Serial.println("Association failed. Waiting 5 seconds.");
    delay(5000);
  }
  Serial.println("Joined!");
  startTime = millis();
}

void loop()
{
  int flexiForceReading = analogRead(flexiForcePin);
  Serial.println(flexiForceReading);
  delay(200);
  
  int now = millis();
  if (now > startTime + 5000) {
    int flexiForceReading = analogRead(flexiForcePin);
  
    Serial.println(flexiForceReading);
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print(flexiForceReading);
    lcd.clear();
    if(flexiForceReading > high){
      lcd.print("Drink me!");
    }else if(flexiForceReading > medium){
      lcd.print("medium");
    }else{
      lcd.print("Refill the keg!");
    }
    
    if(client.connected() || client.connect()) 
    {
      Serial.println("Sending GET request. v=" + String(flexiForceReading));
      client.println( "GET /beer_statuses/new?v=" + String(flexiForceReading) + " HTTP/1.0" );
      client.println( "Accept:text/html,application/xhtml+xml,application/xml;q=0.9,image/webp;*;q=0.8");
      client.println( "Accept-Encoding:gzip,deflate,sdch");
      client.println( "Accept-Language:en-US,en;q=0.8");
      client.println( "Cache-Control:max-age=0");
      client.println( "Connection:keep-alive");
      client.println( "Host:beerstatus.herokuapp.com");
      client.println( "If-None-Match:\"245bd99ca53c3be681fa7d570a12896a\"");
      client.println( "User-Agent:Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/32.0.1700.77 Safari/537.36");
      client.println();
    } 
    else
    {
      Serial.println("Error connecting with remote host");
    }
    startTime = now;
  }
  
  while(client.available())
  {
    char c = client.read();
    Serial.print(c);  
  }
}
