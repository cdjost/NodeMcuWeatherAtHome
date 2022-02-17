#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "wifi_conf.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HTU21D sensor;

// Wifi Signalstärke
int32_t RSSI;

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setup() {
 Serial.begin(9600); /* begin serial for debug */
 Wire.begin(D1, D2); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  
  delay(2000);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wifi Network");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.setTimeOffset(3600);
  
  display.clearDisplay();

  sensor.begin();
}

void loop() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
  
  display.clearDisplay();
   
  RSSI = WiFi.RSSI();
  display.fillRect(0,0,128,18,WHITE); // x, y, w, z | x,y Startposition von oben links w breite horzontal, z höhe vertikal
  
  if (RSSI > -65) {
  display.fillRect(14,1,2,16,BLACK);
  }
  if (RSSI > -70) {
  display.fillRect(10,5,2,12,BLACK);
  }
  if (RSSI > -78) {
  display.fillRect(6,9,2,8,BLACK);
  }
  if (RSSI > -82) {
  display.fillRect(2,13,2,4,BLACK);
  }

  display.setTextSize(2); // Texthöhe 14 Pixel Breite 10?
  display.setTextColor(BLACK);

  display.setCursor(65,2);
  String currentTime = timeClient.getFormattedTime();
  display.print(currentTime.substring(0,5)); // Sekunden abschneiden
  //display.println(timeClient.getFormattedTime());
  //Serial.print("Formatted time: ");
  //Serial.println(timeClient.getFormattedTime());

  display.setTextSize(1);
  display.setTextColor(WHITE);
 
  if(sensor.measure()) {
    float temperature = sensor.getTemperature();
    float humidity = sensor.getHumidity();
    
    display.setCursor(0,45);
    display.print("Temperatur (C): ");
    display.println(temperature);

    display.print("Luftf.   (%RH): ");
    display.println(humidity);

  }

  display.display();
  delay(5000);
}
