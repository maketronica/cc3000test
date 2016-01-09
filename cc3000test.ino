/***************************************************
  Cloud Data-Logging Thermometer
 
  Log a thermomistor value to an Amazon DynamoDB table every minute.
  
  Copyright 2013 Tony DiCola (tony@tonydicola.com).
  Released under an MIT license: 
    http://opensource.org/licenses/MIT
  Dependencies:
  - Adafruit CC3000 Library 
    https://github.com/adafruit/Adafruit_CC3000_Library
  - RTClib Library
    https://github.com/adafruit/RTClib
  
  Parts of this code were adapted from Adafruit CC3000 library example 
  code which has the following license:
  
  Designed specifically to work with the Adafruit WiFi products:
  ----> https://www.adafruit.com/products/1469
  Adafruit invests time and resources providing this open source code, 
  please support Adafruit and open-source hardware by purchasing 
  products from Adafruit!
  Written by Limor Fried & Kevin Townsend for Adafruit Industries.  
  BSD license, all text above must be included in any redistribution
  
  SHA256 hash and signing code adapted from Peter Knight's library
  available at https://github.com/Cathedrow/Cryptosuite
 ****************************************************/

#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <DHT.h>
#include "config.h"
#include <Adafruit_CC3000_Dynamodb.h>

const byte DHT_PIN = 8;
DHT dht(DHT_PIN, DHT22);

// CC3000 configuration
#define     ADAFRUIT_CC3000_IRQ    3    // MUST be an interrupt pin!
#define     ADAFRUIT_CC3000_VBAT   5    // VBAT and CS can be any two pins
#define     ADAFRUIT_CC3000_CS     10
// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, 
                                         ADAFRUIT_CC3000_IRQ, 
                                         ADAFRUIT_CC3000_VBAT,
                                         SPI_CLOCK_DIV2);

// Wireless network configuration
#define     WLAN_SECURITY          WLAN_SEC_WPA2  // Security can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2

// DynamoDB table configuration
#define     TABLE_NAME             "Temperatures"  // The name of the table to write results.
#define     ID_VALUE               "Test"          // The value for the ID/primary key of the table.  Change this to a 
                                                   // different value each time you start a new measurement.


// Other sketch configuration
#define     READING_DELAY_MINS     1      // Number of minutes to wait between readings.


// State used to keep track of the current time and time since last temp reading.
unsigned long lastPolledTime = 0;   // Last value retrieved from time server
unsigned long sketchTime = 0;       // CPU milliseconds since last server query
unsigned long lastReading = 0;      // Time of last temperature reading.

void setup(void) {
  Serial.begin(9600);
  Serial.println("Starting...");
  
  // Initialize and connect to the wireless network
  // This code is adapted from CC3000 example code.
  if (!cc3000.begin()) {
    Serial.println(F("Unable to initialise the CC3000!"));
    while(1);
  }
  if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
    Serial.println(F("Failed to connect to AP!"));
    while(1);
  }
  // Wait for DHCP to complete
  while (!cc3000.checkDHCP()) {
    delay(100);
  }
  
  // Get an initial time value by querying an NTP server.
  unsigned long t = getTime(cc3000);
  while (t == 0) {
    // Failed to get time, try again in a minute.
    delay(60*1000);
    t = getTime(cc3000);
  }
  lastPolledTime = t;
  sketchTime = millis();

  dht.begin();
  
  Serial.println(F("Running..."));
}

void loop(void) {
  // Update the current time.
  // Note: If the sketch will run for more than ~24 hours, you probably want to query the time
  // server again to keep the current time from getting too skewed.
  unsigned long currentTime = lastPolledTime + (millis() - sketchTime) / 1000;
  
  if ((currentTime - lastReading) >= (READING_DELAY_MINS*60)) {
    lastReading = currentTime;

    // Get a temp reading
    //float currentTemp = readTemp();
  
    // Write the result to the database.
    dynamoDBWrite(cc3000, AWS_ACCESS_KEY, AWS_SECRET_ACCESS_KEY, AWS_REGION, AWS_HOST, TABLE_NAME, ID_VALUE, currentTime, dht.readTemperature());
  }
}

