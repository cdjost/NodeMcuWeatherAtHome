#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
#include "wifi_conf.h"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HTU21D sensor;

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
  
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  sensor.begin();
}

void loop() {
  if(sensor.measure()) {
    float temperature = sensor.getTemperature();
    float humidity = sensor.getHumidity();
    
    display.clearDisplay();
    display.setCursor(0,0);
    display.print("Temperature (C): ");
    display.println(temperature);

    display.print("Humidity (%RH): ");
    display.println(humidity);
    // display.display();

  }

  delay(5000);
}
