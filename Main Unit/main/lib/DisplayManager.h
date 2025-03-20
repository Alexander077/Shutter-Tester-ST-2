#pragma once

#include <Arduino.h>
#include <Wire.h>
// #include <SafeString.h>
#include <mString.h>
#include "MeasuredResult.h"
#include "Common.h"

#define MAIN_BUF_SIZE 200
#define TMP_BUF_SIZE 32

#define FLOAT_WIDTH 7
#define FLOAT_DECIMAL_WIDTH 3

class DisplayManager
{
private:

public:

  void drawMainMenu(uint8_t activeModuItemIndex)
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::MAIN_MENU;
    // mainBuf += ':';
    // char numBuf[10];
    // snprintf(numBuf, sizeof(numBuf), "%d", activeModuItemIndex);
    // mainBuf += numBuf;

    // sendBuf();
  }

  void drawAboutScreen()
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::ABOUT;

    // sendBuf();
  }

  void drawMeasuringScreen()
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::MEASURING;

    // sendBuf();
  }

  void drawCurtainMovementSelectionScreen(uint8_t curSelectedMenuIndex)
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::CURTAN_MOVEMENT_SELECTION;
    // mainBuf += ":";
    // mainBuf.add(curSelectedMenuIndex);

    // sendBuf();
  }

  void drawLightCheckScreen(bool isLightQualityOk, uint8_t sensor0Val, uint8_t sensor1Val)
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::LIGHT_CHECK;
    // mainBuf += ":";
    // mainBuf.add(isLightQualityOk);
    // mainBuf += ":";
    // mainBuf.add(sensor0Val);
    // mainBuf += ":";
    // mainBuf.add(sensor1Val);

    // sendBuf();
  }

  void drawMeasuredScreen(int8_t resultPageIndex, const MeasuredResult results)
  {
    // #define BUF_SIZE 40
    // mainBuf.clear();
    // mainBuf += (char)Screens::MEASURED;
    // mainBuf += ":";

    // char tmpBuf[BUF_SIZE];

    // snprintf(tmpBuf, sizeof(tmpBuf), "%d", resultPageIndex);
    // mainBuf += tmpBuf;
    // mainBuf += /* resultPageIndex != 3 ?  */ ":" /*  : "" */;

    // switch (resultPageIndex)
    // {
    // case 0: // sensor 0
    // {
    //   dtostrf(results.sensor0Time, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   break;
    // }
    // case 1: // sensor 1
    // {
    //   dtostrf(results.sensor1Time, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   break;
    // }
    // case 2: // curtains
    // {
    //   dtostrf(results.curtain1spanAspeed, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.curtain1spanAtime, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.curtain1TotalTime, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.curtain2spanAspeed, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.curtain2spanAtime, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.curtain2TotalTime, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;

    //   break;
    // }
    // case 3:
    // {
    //   dtostrf(results.slitWidthSensor0, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.slitWidthSensor1, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   dtostrf(results.slitWidthAverage, FLOAT_WIDTH, FLOAT_DECIMAL_WIDTH, tmpBuf);
    //   mainBuf += tmpBuf;
    //   mainBuf += ":";

    //   mainBuf.add((uint8_t)results.usedSensorType);
    //   mainBuf += ":";

    //   mainBuf.add((uint8_t)results.selectedCurtainMovement);

    //   break;
    // }

    // default:
    //   break;
    // }

    // sendBuf();
  }

  void drawMeasurementResultRecordSelectionScreen(int16_t curSelectedIndex, int32_t recordNumbers[], int16_t recordNumbersCount)
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::MEASUREMENT_RECORD_SELECTION;
    // mainBuf += ":";
    // mainBuf.add(curSelectedIndex);
    // mainBuf += ":";
    // mainBuf.add(recordNumbersCount);

    // for (int16_t i = 0; i < recordNumbersCount; i++)
    // {
    //   mainBuf += ":";
    //   mainBuf.add(recordNumbers[i]);
    // }

    // sendBuf();
  }

  void sendRawEncoder(int16_t encoderVal)
  {
    // mainBuf[0] = '\0';
    // char bufScreenName[3] = {(char)Screens::TEST, ':', '\0'};
    // strcat(mainBuf, bufScreenName);
    // char numBuf[10];
    // snprintf(numBuf, sizeof(numBuf), "%d", encoderVal);
    // strcat(mainBuf, numBuf);

    // sendBuf();
  }

  void drawMeasurementSaveScreen(uint8_t curSelectedMenuIndex, int16_t freeSlotsLeft, bool drawMoreThanZeroRecordsMenuItems)
  {
    // mainBuf.clear();
    // mainBuf += (char)Screens::MEASUREMENT_SAVE_SCREEN;
    // mainBuf += ":";
    // mainBuf.add(curSelectedMenuIndex);
    // mainBuf += ":";
    // mainBuf.add(freeSlotsLeft);
    // mainBuf += ":";
    // mainBuf.add((int8_t)drawMoreThanZeroRecordsMenuItems);

    // sendBuf();
  }

  void drawMessageScreen(int32_t messageScreenId, bool useProgmem, const char *message, const char *options[], int16_t optionsCount, int16_t selectedOptionIndex)
  {
    // const __FlashStringHelper *errStr = F("Halted. Not allowed character: ':'");

    // if (useProgmem && strchr_P(message, ':'))
    // {
    //   halt(errStr);
    // }

    // if (!useProgmem && strchr(message, ':'))
    // {
    //   halt(errStr);
    // }

    // mainBuf.clear();
    // mainBuf += (char)Screens::MESSAGE;
    // mainBuf += ":";
    // mainBuf.add(messageScreenId);
    // mainBuf += ":";
    // mainBuf.add(optionsCount);
    // mainBuf += ":";
    // mainBuf.add(selectedOptionIndex);
    // mainBuf += ":";

    // if (useProgmem)
    // {
    //   mainBuf.add_P(message);
    // }
    // else
    // {
    //   mainBuf.add(message);
    // }

    // for (int16_t i = 0; i < optionsCount; i++)
    // {
    //   if ((useProgmem && strchr_P(options[i], ':')) || (!useProgmem && strchr(options[i], ':')))
    //   {
    //     halt(F("Halted. Not allowed character in options: ':'"));
    //   }
    //   else
    //   {
    //     mainBuf += ":";

    //     if (useProgmem)
    //     {
    //       mainBuf.add_P(options[i]);
    //     }
    //     else
    //     {
    //       mainBuf.add(options[i]);
    //     }
    //   }
    // }

    // sendBuf();
  }
};

#undef BUF_SIZE
#undef MAIN_BUF_SIZE
#undef TMP_BUF_SIZE
#undef FLOAT_WIDTH
#undef FLOAT_DECIMAL_WIDTH