#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <HTU21D.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
HTU21D sensor;

float temperature = 100;
float humidity = -100;

void setup() {
 Serial.begin(9600); /* begin serial for debug */
 Wire.begin(D1, D2); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */

 initDisplay();

  sensor.begin();
}

void loop() {
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
  if(sensor.measure()){
    temperature = sensor.getTemperature();
    humidity = sensor.getHumidity();
  }
}

void renderDisplay() {

  display.clearDisplay();
  display.dim(true);

  display.drawLine(0, 14, SCREEN_WIDTH, 14, WHITE);
     
  display.setTextSize(3);
  display.setCursor(0, (SCREEN_HEIGHT/2) - 10);
    
  display.print(temperature);
  display.println(" C");

  display.drawLine(0, 50, SCREEN_WIDTH, 50, WHITE);
    
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("(%RH): ");
  display.println(humidity);

  display.display();
}
