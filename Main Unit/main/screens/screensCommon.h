#pragma once

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "lib/Common.h"
#include "lib/MeasuredResult.h"
#include "lib/Display.h"
#include "lib/SerialAPICommon.h"

uint16_t sensor0Max = 0;
uint16_t sensor1Max = 0;


struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};


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

enum class SeriesField : uint8_t
{
  SENSOR0_TIME = 0,
  SENSOR1_TIME,
  CURTAIN1_SPAN_A_TIME,
  CURTAIN1_SPAN_A_SPEED,
  CURTAIN1_TOTAL_TIME,
  CURTAIN2_SPAN_A_TIME,
  CURTAIN2_SPAN_A_SPEED,
  CURTAIN2_TOTAL_TIME,
  SLIT_WIDTH_SENSOR0,
  SLIT_WIDTH_SENSOR1,
  COUNT
};

struct SeriesFieldDescriptor
{
  const char *tableLabel; // short label for the summary table row
  const char *chartTitle; // title of the chart page
  bool displayAsSpeed;    // sensor time fields are displayed as shutter speed 1/X on charts
};

#define SERIES_FIELDS_COUNT ((uint8_t)SeriesField::COUNT)

const SeriesFieldDescriptor seriesFieldDescriptors[SERIES_FIELDS_COUNT] =
    {
        {"S1 time ms", "Sensor 1 speed", true},
        {"S2 time ms", "Sensor 2 speed", true},
        {"C1 span ms", "Curtain 1 span time", false},
        {"C1 spd m/s", "Curtain 1 speed", false},
        {"C1 tot ms", "Curtain 1 total time", false},
        {"C2 span ms", "Curtain 2 span time", false},
        {"C2 spd m/s", "Curtain 2 speed", false},
        {"C2 tot ms", "Curtain 2 total time", false},
        {"Slit S1 mm", "Slit width sensor 1", false},
        {"Slit S2 mm", "Slit width sensor 2", false},
};

#define MEASUREMENT_SERIES_MAX_SIZE 10

// Available series sizes (1 = single measurement, current behavior)
const uint8_t measurementSeriesOptions[] = {1, 2, 3, 5, 7, 10};
const uint8_t measurementSeriesOptionsCount = sizeof(measurementSeriesOptions) / sizeof(measurementSeriesOptions[0]);

struct MeasurementSeries
{
  uint8_t totalCount;  // how many measurements the series consists of
  uint8_t storedCount; // how many results are actually stored in results[]
  MeasuredResult results[MEASUREMENT_SERIES_MAX_SIZE];
};

// Fields of MeasuredResult that take part in the series statistics and charts

// Special (sentinel) values of MeasuredResult are negative, real measurements
// are always positive. inf/NaN (e.g. from a zero curtain time glitch) are also
// treated as invalid.
bool isSeriesFieldValueValid(double value)
{
  return value > 0 && isfinite(value);
}

// Returns the value of the given MeasuredResult field. Shared by the series
// summary table, statistics and chart pages.
double getSeriesFieldValue(const MeasuredResult &res, SeriesField field)
{
  switch (field)
  {
  case SeriesField::SENSOR0_TIME:
    return res.sensor0Time;
  case SeriesField::SENSOR1_TIME:
    return res.sensor1Time;
  case SeriesField::CURTAIN1_SPAN_A_TIME:
    return res.curtain1spanAtime;
  case SeriesField::CURTAIN1_SPAN_A_SPEED:
    return res.curtain1spanAspeed;
  case SeriesField::CURTAIN1_TOTAL_TIME:
    return res.curtain1TotalTime;
  case SeriesField::CURTAIN2_SPAN_A_TIME:
    return res.curtain2spanAtime;
  case SeriesField::CURTAIN2_SPAN_A_SPEED:
    return res.curtain2spanAspeed;
  case SeriesField::CURTAIN2_TOTAL_TIME:
    return res.curtain2TotalTime;
  case SeriesField::SLIT_WIDTH_SENSOR0:
    return res.slitWidthSensor0;
  case SeriesField::SLIT_WIDTH_SENSOR1:
    return res.slitWidthSensor1;
  default:
    return 0;
  }
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

// Builds the list of series fields that have at least one valid value in the
// series. Fields unavailable for the whole series (e.g. curtain and slit data
// for leaf shutters) are excluded so the report has no empty pages.
uint8_t buildActiveSeriesFields(const MeasurementSeries &series, SeriesField *activeFields)
{
  uint8_t activeCount = 0;

  for (uint8_t f = 0; f < SERIES_FIELDS_COUNT; f++)
  {
    for (uint8_t i = 0; i < series.storedCount; i++)
    {
      if (isSeriesFieldValueValid(getSeriesFieldValue(series.results[i], (SeriesField)f)))
      {
        activeFields[activeCount++] = (SeriesField)f;
        break;
      }
    }
  }

  return activeCount;
}

// Draws text with its right edge at rightX (uses the text bounding box width)
void drawStringRightAligned(const char *str, int16_t rightX, int16_t y)
{
  Rect r = getStringRect(str, 0, 0);
  display->setCursor(rightX - r.width, y);
  display->print(str);
}

// Defined in screens/MeasuredResult.h; declared here so that SeriesReport.h
// does not depend on the header include order.
void drawNavBar(int16_t activePageIndex, int16_t itemsCount);