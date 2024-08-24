#pragma once

#include <Arduino.h>
#include <Wire.h>

enum class Screens
{
  MAIN_MENU = 'm',
  MEASURING = 'n',
  MEASURED = 'r',
  CREDITS = 'c',
  TEST = 't',
};

class DisplayManager
{
private:
  const uint8_t displayI2CAddress = 4;
  char buf[30];
  void sendBuf()
  {
    // Serial.println(buf);
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

  void drawMeasuredScreen(int8_t resultPageIndex, const char * param)
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::MEASURED, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", resultPageIndex);
    strcat(buf, numBuf);
    strcat(buf, ":");
    strcat(buf, param);

    sendBuf();
  }

  void drawMeasuredScreen(int8_t resultPageIndex, const String param)
  {
    buf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::MEASURED, ':', '\0'};
    strcat(buf, bufScreenName);
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", resultPageIndex);
    strcat(buf, numBuf);
    strcat(buf, ":");
    strcat(buf, param.c_str());

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

