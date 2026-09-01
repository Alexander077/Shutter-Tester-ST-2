#pragma once

#include <string.h>
#include <stdio.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "lib/Common.h"
#include "lib/MeasuredResult.h"
#include "lib/Display.h"
#include "lib/SerialAPICommon.h"

uint16_t sensor0Max = 0;
uint16_t sensor1Max = 0;

struct ComputedMeasurement
{
  MeasuredResult res;
  bool sensor0DataOk;
  bool sensor1DataOk;
};

enum class MeasurementSaveScreenResult
{
  OK,
  CANCEL
};

enum class CurtainMovementSelectionScreenResult
{
  GO_BACK,
  GO_TO_MAIN_MENU
};

const char *MainMenuItemsStr[MAIN_MENU_ITEMS_COUNT] =
    {
        "Measure",
        "Light Setup",
        "Saved Measures",
        "About"};

const char *CurtainMovementItemsStr[CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT] =
    {
        "Horisontal",
        "Vertical",
        "Leaf"};

const char *MeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ITEMS_COUNT] =
    {
        "No",
        "Yes",
        "Overwrite oldest",
        "Overwrite newest",
        "Sel. rec. to overwrite",
        "Return to result",
};

const char *ZeroRecordsMeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT] =
    {
        "No",
        "Yes",
        "Return to result",
};

struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

struct RawMeasurementCapture
{
  bool isCancelled;
  uint32_t sensor0PulseWidthUs;
  uint32_t sensor1PulseWidthUs;
  uint32_t curtain1TimeUs;
  uint32_t curtain2TimeUs;
};

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

const char *GetCurtainMovementItemTitle(CurtainMovement menuItem)
{
  return CurtainMovementItemsStr[(uint8_t)menuItem];
}

const char *GetSaveMasurementMenuItemTitle(MeasurementSaveMenuItems menuItem, bool useForZeroRecords)
{
  if (useForZeroRecords)
  {
    return ZeroRecordsMeasurementSaveItemsStr[(uint8_t)menuItem];
  }
  else
  {
    return MeasurementSaveItemsStr[(uint8_t)menuItem];
  }
}

Rect getStringRect(const char *str, int16_t x, int16_t y)
{
  // Variables to hold the bounding box dimensions
  int16_t x1, y1;
  uint16_t w, h;

  // Get the bounds of the text
  display->getTextBounds(str, x, y, &x1, &y1, &w, &h);

  Rect r;
  r.x = x1;
  r.y = y1;
  r.width = w;
  r.height = h;
  return r;
}

Rect drawStringHCentered(const char *str, uint16_t y)
{
  // // Variables to hold the bounding box dimensions
  // int16_t x1, y1;
  // uint16_t w, h;

  Rect r = getStringRect(str, 0, y);
  // Serial.println(r.y);

  // Calculate the position to center the text on the screen
  int16_t x = (display->width() - r.width) / 2;
  // int16_t y = (display->height() - h) / 2;
  r.x = x;

  // Draw the text at the calculated position
  display->setCursor(x, y);
  display->print(str);

  // Serial.print(r.x);
  // Serial.print("|");
  // Serial.print(r.y);
  // Serial.print("|");
  // Serial.print(r.width);
  // Serial.print("|");
  // Serial.println(r.height);

  // Optional: Draw the bounding box around the text for visualization
  // display->drawRect(x, y - r.height, r.width, r.height, RED);
  return r;
}

Rect drawStringHCentered(const String &str, uint16_t y)
{
#define CHAR_BUF_SIZE 50
  char buf[CHAR_BUF_SIZE];
  str.toCharArray(buf, CHAR_BUF_SIZE);
  return drawStringHCentered(buf, y);
}

void drawNavBar(int16_t activePageIndex, int16_t itemsCount)
{
  int16_t navBarWidth = 120;
  int16_t navBarY = 120;
  // int16_t navBarItemsCount = RESULT_PAGES_COUNT;
  int16_t navBarLeftMargin = (display->width() - navBarWidth) / 2;
  display->drawLine(navBarLeftMargin, navBarY, navBarLeftMargin + navBarWidth, navBarY, WHITE);

  double spacingStep = (double)navBarWidth / (double)(itemsCount - 1);

  for (int8_t i = 0; i < itemsCount; i++)
  {
    int16_t spacing = (int16_t)(i * spacingStep);
    display->fillCircle(navBarLeftMargin + spacing, navBarY, i == activePageIndex ? 4 : 2, i == activePageIndex ? RGB565(0, 102, 153) : WHITE);
  }
}

void drawSensorSignalLevelBar(uint8_t &x, uint8_t &y, uint8_t sensorNumber, uint16_t curSensorValue)
{
  static int8_t prevSensor1SignalStatus = -1;
  static int8_t prevSensor2SignalStatus = -1;
  static int8_t prevSensor1ActualBarWidth = 0;
  static int8_t prevSensor2ActualBarWidth = 0;
  static const char *sensorStatuses[3] = {"Too dim", "Too bright", "OK"};
  const uint8_t barWidthPx = 80;
  const uint8_t barX = 10;
  double sensorValueRate = curSensorValue / (double)MAX_SIGNAL_LEVEL;
  sensorValueRate = sensorValueRate > 1.0 ? 1.0 : sensorValueRate;
  x = 15;
  y += 20;
  display->setCursor(x, y);
  display->printf("Sensor %i signal level", sensorNumber);
  y += 5;
  display->drawRect(barX, y, barWidthPx, 10, WHITE);
  int16_t newBarWidth = (barWidthPx - 2) * sensorValueRate;

  if (sensorNumber == 1 && newBarWidth != prevSensor1ActualBarWidth)
  {
    if (newBarWidth > prevSensor1ActualBarWidth) // add to what we have
    {
      display->fillRect(barX + 1 + prevSensor1ActualBarWidth, y + 1, newBarWidth - prevSensor1ActualBarWidth, 8, WHITE);
    }
    else // draw black upon what we have
    {
      display->fillRect(barX + 1 + prevSensor1ActualBarWidth - (prevSensor1ActualBarWidth - newBarWidth),
                        y + 1, prevSensor1ActualBarWidth - newBarWidth, 8, BLACK);
    }

    prevSensor1ActualBarWidth = newBarWidth;
  }

  if (sensorNumber == 2 && newBarWidth != prevSensor2ActualBarWidth)
  {
    if (newBarWidth > prevSensor2ActualBarWidth) // add to what we have
    {
      display->fillRect(barX + 1 + prevSensor2ActualBarWidth, y + 1, newBarWidth - prevSensor2ActualBarWidth, 8, WHITE);
    }
    else // draw black upon what we have
    {
      display->fillRect(barX + 1 + prevSensor2ActualBarWidth - (prevSensor2ActualBarWidth - newBarWidth),
                        y + 1, prevSensor2ActualBarWidth - newBarWidth, 8, BLACK);
    }

    prevSensor2ActualBarWidth = newBarWidth;
  }

  x += barWidthPx;
  y += 9;

  int8_t sensorStatus = 0;

  if (curSensorValue <= MIN_ALLOWED_SIGNAL_LEVEL)
  {
    sensorStatus = 0;
  }
  else if (curSensorValue > MAX_ALLOWED_SIGNAL_LEVEL)
  {
    sensorStatus = 1;
  }
  else
  {
    sensorStatus = 2;
  }

  if (sensorNumber == 1 && sensorStatus != prevSensor1SignalStatus)
  {
    display->setTextColor(BLACK); // erase old text
    display->setCursor(x, y);
    display->print(sensorStatuses[prevSensor1SignalStatus == -1 ? 0 : prevSensor1SignalStatus]);

    prevSensor1SignalStatus = sensorStatus;
  }

  if (sensorNumber == 2 && sensorStatus != prevSensor2SignalStatus)
  {
    display->setTextColor(BLACK); // erase old text
    display->setCursor(x, y);
    display->print(sensorStatuses[prevSensor2SignalStatus == -1 ? 0 : prevSensor2SignalStatus]);

    prevSensor2SignalStatus = sensorStatus;
  }

  display->setCursor(x, y);
  display->setTextColor(WHITE);
  display->print(sensorStatuses[sensorStatus]);
}

// Formats a positive value compactly so it fits into the narrow summary table columns
void formatCompactDouble(double value, char *buf, size_t bufSize)
{
  if (value >= 1000)
  {
    snprintf(buf, bufSize, "%.0f", value);
  }
  else if (value >= 100)
  {
    snprintf(buf, bufSize, "%.1f", value);
  }
  else
  {
    snprintf(buf, bufSize, "%.2f", value);
  }
}
