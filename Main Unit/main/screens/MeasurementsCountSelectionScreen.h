#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "lib/AlexEncoder.h"
#include "lib/serialAPI.h"

// Asks the user how many measurements to take in the series.
// Returns 0 if 'Go Back' was selected (or on API request), otherwise the selected count.
uint8_t drawMeasurementsCountSelectionScreen()
{
  const uint8_t optionsCount = measurementSeriesOptionsCount + 1; // +1 for 'Go Back'
  const char *options[measurementSeriesOptionsCount + 1];
  char countLabels[measurementSeriesOptionsCount][5];

  for (uint8_t i = 0; i < measurementSeriesOptionsCount; i++)
  {
    snprintf(countLabels[i], sizeof(countLabels[i]), "%u", (unsigned)measurementSeriesOptions[i]);
    options[i] = countLabels[i];
  }

  options[measurementSeriesOptionsCount] = "Go Back";

  int16_t startEncoderVal = AlexEncoder::counter;
  int8_t prevSelectedMenuItemIndex = 0;

  uint8_t xMargin = 35;
  uint8_t yMargin = 35;
  uint8_t ySpacing = 12;
  uint8_t arrowXmargin = 15;

  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  drawStringHCentered("Measurements count", 20);
  display->setFont();

  for (uint8_t i = 0; i < optionsCount; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));
    display->print(options[i]);
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  while (true)
  {
    if (isApiRequestReceived)
      return 0;

    int16_t resultOptionIndex = AlexEncoder::counter - startEncoderVal;

    if (resultOptionIndex > optionsCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (optionsCount - 1);
      resultOptionIndex = optionsCount - 1;
    }

    if (resultOptionIndex < 0)
    {
      resultOptionIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    if (prevSelectedMenuItemIndex != resultOptionIndex)
    {
      display->setTextColor(BLACK);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
      display->print("->");

      display->setTextColor(WHITE);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultOptionIndex));
      display->print("->");

      prevSelectedMenuItemIndex = resultOptionIndex;
    }

    if (button.isClicked())
    {
      if (resultOptionIndex == measurementSeriesOptionsCount) // 'Go Back' selected
      {
        return 0;
      }

      return measurementSeriesOptions[resultOptionIndex];
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}