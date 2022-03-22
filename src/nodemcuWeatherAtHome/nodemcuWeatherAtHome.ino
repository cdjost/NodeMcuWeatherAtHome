#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <HTU21D.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "config.h"
#include <ArduinoOTA.h>

#if ENABLE_CO2
#include <MHZ.h>
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

// NTP
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

WiFiClient wifiClient;

#if ENABLE_MQTT
PubSubClient mqttClient(wifiClient);
#endif

float temperature = 100;
float humidity = -100;
int ppm_uart = -1;

unsigned long air_warn_start_time = 0;
unsigned long lastSensorRead = millis();
unsigned long lastRSSIRead = millis();

bool isDelimiterShowing = true;
int hour = 0;
int minute = 0;

#if ENABLE_CO2
MHZ co2(MH_Z19_RX, MH_Z19_TX, MHZ19B);
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

  timeClient.begin();
  timeClient.setTimeOffset(3600);

  sensor.begin();

  // Initial sensor read
  readSensorData();
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(HOST);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting to Wifi Network");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
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
  timeClient.update();

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

  delay(1000);
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
  display.println("Loading...");
  display.display();
}

void readSensorData() {
  if (sensor.measure()) {
    temperature = sensor.getTemperature();
    humidity = sensor.getHumidity();
  }

#if ENABLE_CO2
  if (co2.isReady()) {
    ppm_uart = co2.readCO2UART();
  }
#endif
}

void getCurrentTime() {
  hour = timeClient.getHours();
  minute = timeClient.getMinutes();
}

void renderDisplay() {
  display.clearDisplay();


  // Turns the display off at night
  if ((hour < 6 || hour >= 23) && !DISABLE_DISPLAY_OFF) {
    if (isDisplayOn) {
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      isDisplayOn = false;
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

  char timeAnimation[6];
  if (isDelimiterShowing) {
    sprintf(timeAnimation, "%02d:%02d", hour, minute);
  } else {
    sprintf(timeAnimation, "%02d %02d", hour, minute);
  }
  isDelimiterShowing = !isDelimiterShowing;
  display.print(timeAnimation);

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
  if (ppm_uart < 0) {
    display.print("n/a ppm");
  }
  else {
    display.printf("%dppm", ppm_uart);
  }
#endif

#if ENABLE_AIR_WARNING
  if (humidity > 60 || ppm_uart > 1000) {
    if (air_warn_start_time == 0) {
      air_warn_start_time = millis();
    }
    // 30 Sekunden lang Warnung anzeigen danach 30 Sekunden lang Messwerte
    if ((millis() - air_warn_start_time) < 30000) {
      display.fillRect(0, 50, SCREEN_WIDTH, 14, WHITE);
      display.setCursor(25, 55);
      display.setTextColor(BLACK);
      display.print("bitte Lueften");
      display.setTextColor(WHITE);
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
