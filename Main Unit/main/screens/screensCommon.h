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
#define SERIES_SUMMARY_PAGES_COUNT 2

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

// Special (sentinel) values of MeasuredResult are negative, real measurements are always positive
bool isSeriesFieldValueValid(double value)
{
  return value > 0;
}