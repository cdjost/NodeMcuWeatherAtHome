#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"
#include <ArduinoOTA.h>
#include <MHZ.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define MH_Z19_RX D7  // D7
#define MH_Z19_TX D6  // D6

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HTU21D sensor;


// Wifi Signalstärke
<<<<<<< HEAD
int32_t RSSI;
=======
int32_t rssi;
>>>>>>> 5d7566bc5c556314075cb9a2a25cdcb9905374d5

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

float temperature = 100;
float humidity = -100;
int ppm_uart = -1;

unsigned long air_warn_start_time = 0;

MHZ co2(MH_Z19_RX, MH_Z19_TX, MHZ19B);

void setup() {
  Serial.begin(9600); /* begin serial for debug */
  Wire.begin(D1, D2); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */

  initDisplay();

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

  ArduinoOTA.setHostname(HOST);
  ArduinoOTA.setPassword(OTA_PASS);
  
  ArduinoOTA.onStart([]() {
    Serial.println("Start updating");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
  });

  ArduinoOTA.onEnd([]() {
    ESP.restart();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    switch(error) {
      case OTA_AUTH_ERROR:
        Serial.println("Auth Failed");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("Begin Failed");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("Connect Failed");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("Receive Failed");
        break;
      case OTA_END_ERROR:
        Serial.println("End Failed");
    }
    
    delay(3000);
    ESP.restart();
  });

  ArduinoOTA.begin();

  timeClient.begin();
  timeClient.setTimeOffset(3600);

  sensor.begin();
}

void loop() {
  ArduinoOTA.handle();
  timeClient.update();
  readSensorData();
  renderDisplay();
  
  delay(5000);
}

void initDisplay() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  
  delay(2000);
  
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(0, (SCREEN_HEIGHT/2) - 10);
  display.println("Loading...");
  display.display();
}

void readSensorData() {
  rssi = WiFi.RSSI();
  if(sensor.measure()){
    temperature = sensor.getTemperature();
    humidity = sensor.getHumidity();
  }
  if(co2.isReady()){
    ppm_uart = co2.readCO2UART();
  }
}

void renderDisplay() {

  display.clearDisplay();
  display.dim(true);

  display.fillRect(0,0,128,18,WHITE); // x, y, w, z | x,y Startposition von oben links w breite horzontal, z höhe vertikal

  if (rssi > -65) {
    display.fillRect(14,1,2,16,BLACK);
  }
  if (rssi > -70) {
    display.fillRect(10,5,2,12,BLACK);
  }
  if (rssi > -78) {
    display.fillRect(6,9,2,8,BLACK);
  }
  if (rssi > -82) {
    display.fillRect(2,13,2,4,BLACK);
  }

  display.setTextSize(2); // Texthöhe 14 Pixel Breite 10?
  display.setTextColor(BLACK);

  display.setCursor(65,2);
  String currentTime = timeClient.getFormattedTime();
  display.print(currentTime.substring(0,5)); // Sekunden abschneiden

  display.setTextColor(WHITE);

  // display.drawLine(0, 14, SCREEN_WIDTH, 14, WHITE);
     
  display.setTextSize(3);
  display.setCursor(2, (SCREEN_HEIGHT/2) - 10);
    
  display.print(temperature);
  display.println(" C");

  display.drawLine(0, 50, SCREEN_WIDTH, 50, WHITE);
    
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.printf("%.2f%%RH", humidity);
  display.setCursor(80, 55);
  if(ppm_uart < 0){
    display.print("n/a ppm");
  }
  else{
    display.printf("%dppm", ppm_uart);
  }

  if(humidity > 60 || ppm_uart > 1000){
    if(air_warn_start_time == 0){
      air_warn_start_time = millis();
    }
    // 30 Sekunden lang Warnung anzeigen danach 30 Sekunden lang Messwerte
    if((millis() - air_warn_start_time) < 30000){
      display.fillRect(0,50,SCREEN_WIDTH,14,WHITE);
      display.setCursor(25, 55);
      display.setTextColor(BLACK);
      display.print("bitte Lueften");
      display.setTextColor(WHITE);
    }
    else if((millis() - air_warn_start_time) > 60000){
      air_warn_start_time = 0;
    }
  }
  else{
    air_warn_start_time = 0;
  }

  display.display();
}
