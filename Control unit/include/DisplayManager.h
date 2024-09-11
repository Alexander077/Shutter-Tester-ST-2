#pragma once

#include <Arduino.h>
#include <Wire.h>
// #include <SafeString.h>
#include <mString.h>
#include "MeasuredResult.h"

#define MAIN_BUF_SIZE 200
#define TMP_BUF_SIZE 32

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
  const uint8_t displayControllerI2CAddress = 4;
  mString<MAIN_BUF_SIZE> mainBuf;

  void sendBuf()
  {
    // Serial.println("Enter sendBuf");

    bool isDataReceivedByScreenController = false;
    bool isDataProcessed = false;

    // while (!isDataReceivedByScreenController)
    // {
      Wire.beginTransmission(displayControllerI2CAddress);
      Wire.write("StArT");
      Wire.endTransmission();
      // delay(500); // Wait for display controller to receive the data

      uint16_t strLength = strlen(mainBuf.c_str());
      char tmpBuf[TMP_BUF_SIZE];

      // Serial.print("Start writing main buf: ");
      // Serial.println(mainBuf.c_str());

      uint16_t startIndex = 0;
      // uint16_t endIndex = TMP_BUF_SIZE - 1;
      do
      {
        // mainBuf.substring(startIndex, endIndex, tmpBuf);
        strncpy(tmpBuf, mainBuf.buf + startIndex, TMP_BUF_SIZE);

        Wire.beginTransmission(displayControllerI2CAddress);
        Wire.write(tmpBuf);
        Wire.endTransmission();

        startIndex += TMP_BUF_SIZE;
        // endIndex += TMP_BUF_SIZE;
      } while (startIndex < strLength);
      // delay(500); // Wait for display controller to receive the data

      // Serial.print("Written ");
      // Serial.print(strLength);
      // Serial.println(" characters");

      Wire.beginTransmission(displayControllerI2CAddress); // transmit to device #4
      Wire.write(("EnD" + String(strLength)).c_str());
      Wire.endTransmission(); // stop transmitting

      Serial.println("Data sent");
      // delay(500); // Wait for display controller to receive the data

      // Wire.requestFrom(displayControllerI2CAddress, 1); // Request read confirmation from display controller
      // while (Wire.available() < 1){} // Wait for display controller to reply
      // isDataReceivedByScreenController = Wire.read();
    // }

    // Serial.println("Data received by display");

    // delay(50); // Wait for display controller to process the data

    // while (!isDataProcessed)
    // {
    //   Wire.requestFrom(displayControllerI2CAddress, 1); // Request 1 byte from slave
    //   while (Wire.available() < 1){} // Wait for display controller to reply
    //   isDataProcessed = Wire.read();
    // }
  }

public:
  void Init()
  {
    Wire.begin(); // join i2c bus (address optional for master)
  }

  void drawMainMenu(uint8_t activeModuItemIndex)
  {
    mainBuf.clear();
    // char bufScreenName[3] = {(char)Screens::MAIN_MENU, ':', '\0'};
    mainBuf += (char)Screens::MAIN_MENU + ":";
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    mainBuf += numBuf;

    sendBuf();
  }

  void drawCreditsScreen()
  {
    mainBuf.clear();
    mainBuf += (char)Screens::CREDITS;

    sendBuf();
  }

  void drawMeasuringScreen()
  {
    mainBuf.clear();
    mainBuf += (char)Screens::MEASURING;

    sendBuf();
  }

  /* void drawMeasuredScreen(int8_t resultPageIndex, const char * param)
  {
    mainBuf[0] = '\0';
    char bufScreenName[3] = {(char)Screens::MEASURED, ':', '\0'};
    strcat(mainBuf, bufScreenName);
    char numBuf[10];
    snprintf(numBuf, sizeof(numBuf), "%d", resultPageIndex);
    strcat(mainBuf, numBuf);
    strcat(mainBuf, ":");
    strcat(mainBuf, param);

    sendBuf();
  } */

  void drawMeasuredScreen(int8_t resultPageIndex, const MeasuredResult results)
  {
    #define BUF_SIZE 40
    mainBuf.clear();
    mainBuf += (char)Screens::MEASURED;
    mainBuf += ":";

    char tmpBuf[BUF_SIZE];

    snprintf(tmpBuf, sizeof(tmpBuf), "%d:", resultPageIndex);
    mainBuf += tmpBuf;

    #define FLOAT_WIDTH 1

    switch (resultPageIndex)
    {
      case 0:
      {
        dtostrf(results.sensor0Time, FLOAT_WIDTH, 2, tmpBuf);
        mainBuf += tmpBuf;
        break;
      }
      case 1:
      {
        dtostrf(results.sensor1Time, FLOAT_WIDTH, 2, tmpBuf);
        mainBuf += tmpBuf;
        break;
      }
      case 2:
      {
        dtostrf(results.sensor2Time, FLOAT_WIDTH, 2, tmpBuf);
        mainBuf += tmpBuf;
        break;
      }
      case 3://spans layout screen
      {
        break;
      }
      case 4:
      {
        dtostrf(results.curtain1spanAspeed, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        mainBuf +=  ":";
        dtostrf(results.curtain1spanBspeed, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        mainBuf +=  ":";
        dtostrf(results.curtain1spanCspeed, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        mainBuf +=  ":";
        dtostrf(results.curtain1spanAtime, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        mainBuf +=  ":";
        dtostrf(results.curtain1spanBtime, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        mainBuf +=  ":";
        dtostrf(results.curtain1spanCtime, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        mainBuf +=  ":";
        dtostrf(results.curtain1FrameAvgSpeed, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf += tmpBuf;
        mainBuf += ":";
        dtostrf(results.curtain1TotalTime, FLOAT_WIDTH, 2,tmpBuf);
        mainBuf +=  tmpBuf;
        break;
      }

    default:
      break;
    }

    
    // mainBuf +=  ":";

    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2spanAspeed);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2spanBspeed);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2spanCspeed);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2spanAtime);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2spanBtime);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2spanCtime);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2TotalTime);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2FrameAvgSpeed);
    // strcat(mainBuf, tmpBuf);
    // sprintf(tmpBuf, "%*.3f:", BUF_SIZE - 1, results.curtain2TotalTime);
    // strcat(mainBuf, tmpBuf);

    sendBuf();
  }

  void sendRawEncoder(int16_t encoderVal)
  {
    // mainBuf[0] = '\0';
    // char bufScreenName[3] = {(char)Screens::TEST, ':', '\0'};
    // strcat(mainBuf, bufScreenName);
    // char numBuf[10];
    // snprintf(numBuf, sizeof(numBuf), "%d", encoderVal);
    // strcat(mainBuf, numBuf);

    sendBuf();
  }
};

#undef BUF_SIZE
#undef MAIN_BUF_SIZE