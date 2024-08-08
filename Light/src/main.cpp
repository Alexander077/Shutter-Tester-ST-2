#include <Arduino.h>
#include <HardwareSerial.h>
#include <Adafruit_INA219.h>
#include "../../include/AlexEncoder.h"

#define DAC_CH1 17

HardwareSerial serialPort(1); // Use UART1
Adafruit_INA219 ina219;
float busvoltage = 0;
float current_mA = 0;
float power_mW = 0;

void setup() {
  AlexEncoder::init(39, 40);
  ina219.begin();
  Serial.begin(115200);
  serialPort.begin(115200, SERIAL_8N1, 3, 4); // Baud rate, mode, RX pin, TX pin
}

void loop() {

  if (serialPort.available())
  {
    String receivedData = serialPort.readStringUntil('\n');

    if (receivedData.compareTo("LAT:START"))
    {
      Serial.println("LAT started");

      uint8_t dacVal = 130;

      while (true)
      {
        dacVal += 1;
        dacWrite(DAC_CH1, dacVal);

        current_mA = ina219.getCurrent_mA();
        Serial.print(dacVal);
        Serial.print(":");
        Serial.println(current_mA);
        delay(200);

        if (serialPort.available())
        {
          receivedData = serialPort.readStringUntil('\n');

          if (receivedData.compareTo("LAT:STOP"))
          {
            Serial.println("LAT stopped");
            break;
          }
        }
      }
    }
  }

  return;

  int16_t encVal = AlexEncoder::counter;
  uint8_t dacVal = encVal * 1;
  static int16_t oldDacVal = dacVal;

  if (dacVal >= 0 && dacVal <= 255 && oldDacVal != dacVal)
  {
    dacWrite(DAC_CH1, dacVal);
    oldDacVal = dacVal;
  }

  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  power_mW = ina219.getPower_mW();

  Serial.printf("V: %f", busvoltage);
  Serial.printf(" C: %f", current_mA);
  Serial.printf(" PWR: %f", power_mW);
  Serial.printf(" DAC: %d\n", dacVal);

  delay(100);
}

