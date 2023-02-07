/*
                   # THIS SKETCH IS NOT NEEDED WITH AUTOCALIBRATION #
    Unlike typical sensors, calibration() refers to the zero point where CO2 is 400ppm.
    This 400ppm comes from the average atmospheric value of 400ppm (or atleast was).
    Depending on the sensor model, the hardcoded value can usually be found by
    calling getBackgroundCO2();
    So if you intend to manually calibrate your sensor, it's usually best to do so at
    night and outside after 20 minutes of run time.
    Instead if you're using auto calibration, then the sensor takes the lowest value observed
    in the last 24 hours and adjusts it's self accordingly over a few weeks.
    HOW TO USE:
    ----- Hardware Method  -----
    By pulling the zero HD low (0V) for 7 seconds as per the datasheet.
    ----- Software Method -----
    Run this sketch, disconnect MHZ19 from device after sketch ends (20+ minutes) and upload new
    code to avoid recalibration.
    ----- Auto calibration ----
    As mentioned above if this is set to true, the sensor will adjust it's self over a few weeks
    according to the lowest observed CO2 values each day. *You don't need to run this sketch!
*/

#include <Wire.h>
#include <Arduino.h>
#include "MHZ19.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define RX_PIN 10
#define TX_PIN 11
#define MH_Z19_RX D7  // D7
#define MH_Z19_TX D6  // D6
#define BAUDRATE 9600

MHZ19 myMHZ19;
#if defined(ESP32)
HardwareSerial mySerial(2);                                // On ESP32 we do not require the SoftwareSerial library, since we have 2 USARTS available
#else
#include <SoftwareSerial.h>                                //  Remove if using HardwareSerial or non-uno compatible device
SoftwareSerial mySerial(MH_Z19_RX, MH_Z19_TX);                   // (Uno example) create device to MH-Z19 serial
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

unsigned long timeElapse = 0;

void setup()
{
     Serial.begin(9600); /* begin serial for debug */
  Wire.begin(D1, D2); /* join i2c bus with SDA=D1 and SCL=D2 of NodeMCU */

  initDisplay();

    mySerial.begin(BAUDRATE);    // sensor serial
    myMHZ19.begin(mySerial);     // pass to library

    myMHZ19.autoCalibration(false);     // make sure auto calibration is off
    Serial.print("ABC Status: "); myMHZ19.getABC() ? Serial.println("ON") :  Serial.println("OFF");  // now print it's status

display.clearDisplay();
display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("Waiting 20 minutes to stabilize...");
    display.setCursor(0, 20);
    display.print("Calibrating");
    display.display();
   /* if you don't need to wait (it's already been this amount of time), remove the 2 lines */
    timeElapse = 12e5;   //  20 minutes in milliseconds
    delay(timeElapse);    //  wait this duration

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("Calibration finished");
    display.display();
    myMHZ19.calibrate();    // Take a reading which be used as the zero point for 400 ppm

}

void loop()
{
    if (millis() - timeElapse >= 2000)  // Check if interval has elapsed (non-blocking delay() equivalent)
    {
        int CO2;
        CO2 = myMHZ19.getCO2();

        Serial.print("CO2 (ppm): ");
        Serial.println(CO2);

        int8_t Temp;    // Buffer for temperature
        Temp = myMHZ19.getTemperature();    // Request Temperature (as Celsius)

        Serial.print("Temperature (C): ");
        Serial.println(Temp);

        timeElapse = millis();  // Update interval
    }
}

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