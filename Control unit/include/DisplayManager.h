#pragma once

#include <Arduino.h>
#include <Wire.h>

enum class Screens
{
  MAIN_MENU = 'm',
  MEASURING = 'n',
  MEASURED = 'd',
  CREDITS = 'c',
  TEST = 't',
};

class DisplayManager
{
private:
  const uint8_t displayI2CAddress = 4;
  char buf[20];
  void sendBuf()
  {
    Serial.println(buf);
    Wire.beginTransmission(displayI2CAddress); // transmit to device #4
    Wire.write(buf);
    Wire.endTransmission(); // stop transmitting
  }
public:
  void Init()
  {
    Wire.begin(); // join i2c bus (address optional for master)
  }

  void drawMainMenu(uint8_t activeModuItemIndex)
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::MAIN_MENU, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    strcat(buf, numBuf);

    sendBuf();
  }

  void drawCreditsScreen()
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::CREDITS, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    // snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    // strcat(buf, numBuf);
    // Serial.println(buf);

    sendBuf();
  }

  void drawMeasuringScreen()
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::MEASURING, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    // snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    // strcat(buf, numBuf);
    // Serial.println(buf);

    sendBuf();
  }

  void drawMeasuredScreen()
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::MEASURED, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    // snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    // strcat(buf, numBuf);
    // Serial.println(buf);

    sendBuf();
  }

  void sendRawEncoder(int16_t encoderVal)
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::TEST, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", encoderVal);
    strcat(buf, numBuf);

    sendBuf();
  }
};

