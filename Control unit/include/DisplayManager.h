#pragma once

#include <Arduino.h>
#include <Wire.h>

// enum class AppState
// {
//   START,
//   MEASURING,
//   MEASURED,
//   SENSOR_READINGS_CHECK,
//   PWM_LIGHT_CHECK,
//   // FAST_MEASURING,
//   // FAST_MEASURED
// };

class DisplayManager
{
private:
  const uint8_t displayI2CAddress = 4;
  char buf[20];
public:
  void Init()
  {
    Wire.begin(); // join i2c bus (address optional for master)
  }

  void drawMainMenu(uint8_t activeModuItemIndex)
  {
    buf[0] = '\0';
    strcat(buf, "m:");
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    strcat(buf, numBuf);
    Serial.println(buf);

    Wire.beginTransmission(displayI2CAddress); // transmit to device #4
    Wire.write(buf);
    Wire.endTransmission();    // stop transmitting
  }
};

