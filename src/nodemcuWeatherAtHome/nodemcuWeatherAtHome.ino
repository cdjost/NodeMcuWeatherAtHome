#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"
#include <ArduinoOTA.h>
#include <time.h>

#if ENABLE_PRESSURE
#include <Adafruit_BMP280.h>
#endif

#if ENABLE_CO2
#include "MHZ19.h"
#endif

#if ENABLE_MQTT
#include <PubSubClient.h>
#include <ArduinoJson.h>
#endif

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define MH_Z19_RX D7  // D7
#define MH_Z19_TX D6  // D6

const int SENSOR_READ_THRESHOLD = 30000;
const int RSSI_READ_THRESHOLD = 3000;
const bool DISABLE_DISPLAY_OFF = false;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool isDisplayOn = false;

HTU21D sensor;


// Wifi Signalstärke
int32_t rssi;
// Wifi connection check
unsigned long previousMillis = 0;
unsigned long interval = 30000;

// NTP
time_t now;
tm tm;

#if ENABLE_PRESSURE
Adafruit_BMP280 bmp;
#endif

WiFiClient wifiClient;

#if ENABLE_MQTT
PubSubClient mqttClient(wifiClient);
#endif

float temperature = 100;
float humidity = -100;
int ppm_uart = -1;
float pressure = -1;

unsigned long air_warn_start_time = 0;
unsigned long lastSensorRead = millis();
unsigned long lastRSSIRead = millis();

bool isDelimiterShowing = true;
int hour = 0;
int minute = 0;

#if ENABLE_CO2
MHZ19 co2MHZ19;
SoftwareSerial co2Serial(MH_Z19_RX, MH_Z19_TX);
unsigned long preHeatTimer = 0;
#endif

void setup() {
  Serial.begin(9600); /* begin serial for debug */
  Wire.begin(D1, D2); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */

  initDisplay();
  isDisplayOn = true;

  setupWiFi();
  setupOTA();

#if ENABLE_MQTT
  mqttClient.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  connectMQTT();
#endif

#if ENABLE_CO2
  co2Serial.begin(9600); 
  co2MHZ19.begin(co2Serial);
  static byte timeout = 0;
#endif

#if ENABLE_PRESSURE
  unsigned status;
  status = bmp.begin();
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                      "try a different address!"));
    Serial.print("SensorID was: 0x"); Serial.println(bmp.sensorID(),16);
    Serial.print("        ID of 0xFF probably means a bad address, a BMP 180 or BMP 085\n");
    Serial.print("   ID of 0x56-0x58 represents a BMP 280,\n");
    Serial.print("        ID of 0x60 represents a BME 280.\n");
    Serial.print("        ID of 0x61 represents a BME 680.\n");
    while (1) delay(10);
  }

  /* Default settings from datasheet. */
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
#endif

  configTime(TIME_ZOME, NTP_SERVER);

  sensor.begin();

  // Initial sensor read
  readSensorData();
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOST);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wifi Network");
  int maxtries = 5;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
    if(maxtries >= 0)
    {
      maxtries--;
    }
    else
    {
      break;
    }
  } 
  Serial.print("Connected, IP address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
}

void setupOTA() {
  ArduinoOTA.setHostname(HOST);
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([]() {
    Serial.println("Start updating");
    #if ENABLE_MQTT
    mqttClient.disconnect();
    #endif
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r\n", (progress / (total / 100)));
  });

  ArduinoOTA.onEnd([]() {
    ESP.restart();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    switch (error) {
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
}

#if ENABLE_MQTT
void connectMQTT() {
  if (mqttClient.connect(HOST, MQTT_USER, MQTT_PW)) {
    // Serial.println("MQTT connected");
  }
  else {
    Serial.println("Error connecting to MQTT broker");
  }
}
#endif

void loop() {
  ArduinoOTA.handle();

  if (millis() - lastRSSIRead > RSSI_READ_THRESHOLD) {
    rssi = WiFi.RSSI();
    lastRSSIRead = millis();
  }

  if (millis() - lastSensorRead > SENSOR_READ_THRESHOLD ) {
    readSensorData();
#if ENABLE_MQTT
    publishData();
#endif
    lastSensorRead = millis();
  }

  getCurrentTime();
  renderDisplay();

  checkWifi();

  delay(1000);
}

void checkWifi() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >=interval)) {
  Serial.print(millis());
  Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  previousMillis = currentMillis;
}
}

#if ENABLE_MQTT
void publishData() {
#if ENABLE_CO2
  if (ppm_uart < 0) {
    return;
  }
#endif

  StaticJsonDocument<200> doc;
  char jsonPayload[200];
  doc["id"] = HOST;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
#if ENABLE_CO2
  doc["co2"] = ppm_uart;
#endif
#if ENABLE_PRESSURE
  doc["pressure"] = pressure;
#endif

  serializeJson(doc, jsonPayload);

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.publish(MQTT_TOPIC, jsonPayload);
}
#endif

void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }


  delay(2000);

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setTextSize(3);
  display.setCursor(0, (SCREEN_HEIGHT / 2) - 10);
  display.println("Loading");
  display.display();
}

void readSensorData() {
  if (sensor.measure()) {
    temperature = sensor.getTemperature();
    humidity = sensor.getHumidity();
  }

#if ENABLE_CO2
  if (millis() - getDataTimer >= 2000) {
    ppm_uart = co2MHZ19.getCO2();
  }
#endif

#if ENABLE_PRESSURE
  pressure = bmp.readPressure();

  Serial.print(F("Pressure = "));
  Serial.print(pressure);
  Serial.println(" Pa");
#endif

}

void getCurrentTime() {
  time(&now);                       // read the current time
  localtime_r(&now, &tm);           // update the structure tm with the current time
  hour = tm.tm_hour;
  minute = tm.tm_min;
}

void renderDisplay() {
  display.clearDisplay();


  // Turns the display off at night
  // keep display enabled when wifi isn't connected since we can't sync ntp time
  if ((hour < 6 || hour >= 23) && !DISABLE_DISPLAY_OFF && (WiFi.status() == WL_CONNECTED)) {
    if (isDisplayOn) {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      isDisplayOn = false;

      myMHZ19.recoveryReset();                                            // Recovery Reset
      delay(30000);       
      for (byte i = 0; i < 3; i++) {
        myMHZ19.verify();                                              // verification check
          if (myMHZ19.errorCode == RESULT_OK){
            timeout = 0;
            break;
          }
      }
    return;
  }
  else if (!isDisplayOn) {
    display.ssd1306_command(SSD1306_DISPLAYON);
    isDisplayOn = true;
  }

  display.dim(true);

  display.drawLine(0, 18, SCREEN_WIDTH, 18, WHITE);

  if (WiFi.status() == WL_CONNECTED) {
    if (rssi > -65) {
      display.fillRect(14, 1, 2, 16, WHITE);
    }
    if (rssi > -70) {
      display.fillRect(10, 5, 2, 12, WHITE);
    }
    if (rssi > -78) {
      display.fillRect(6, 9, 2, 8, WHITE);
    }
    if (rssi > -82) {
      display.fillRect(2, 13, 2, 4, WHITE);
    }
  }

  display.setTextSize(2); // Texthöhe 14 Pixel Breite 10?
  display.setTextColor(WHITE);

  display.setCursor(65, 2);

  // don't display time if we aren't connected to wifi, since we cant be sure if we are synced with the ntp server
  if((WiFi.status() == WL_CONNECTED))
  {
    char timeAnimation[6];
    if (isDelimiterShowing) {
      sprintf(timeAnimation, "%02d:%02d", hour, minute);
    } else {
      sprintf(timeAnimation, "%02d %02d", hour, minute);
    }
    isDelimiterShowing = !isDelimiterShowing;
    display.print(timeAnimation);
  }

  display.setTextColor(WHITE);

  // display.drawLine(0, 14, SCREEN_WIDTH, 14, WHITE);

  display.setTextSize(3);
  display.setCursor(2, (SCREEN_HEIGHT / 2) - 8);

  display.print(temperature);
  display.println(" C");

  display.drawLine(0, 50, SCREEN_WIDTH, 50, WHITE);

  display.setTextSize(1);
  display.setCursor(0, 55);
  display.printf("%.2f%%RH", humidity);
  display.setCursor(80, 55);

#if ENABLE_CO2
  if (ppm_uart < 400) {
    display.print("n/a ppm");
  }
  else {
    display.printf("%dppm", ppm_uart);
  }
#endif

#if ENABLE_PRESSURE
  if (pressure < 0) {
    display.print("n/a Pa");
  }
  else {
    display.setCursor(60, 55);
    display.printf("%.2fPa", pressure);
  }
#endif

#if ENABLE_AIR_WARNING
  if (humidity > 60 || ppm_uart > 1000) {
    if (air_warn_start_time == 0) {
      air_warn_start_time = millis();
    }
    // 30 Sekunden lang Warnung anzeigen danach 30 Sekunden lang Messwerte
    if ((millis() - air_warn_start_time) < 30000) {
      display.fillRect(0, 51, SCREEN_WIDTH, 14, BLACK);
      display.setCursor(25, 55);
      display.print("bitte Lueften");
    }
    else if ((millis() - air_warn_start_time) > 60000) {
      air_warn_start_time = 0;
    }
  }
  else {
    air_warn_start_time = 0;
  }
#endif

  display.display();
}
