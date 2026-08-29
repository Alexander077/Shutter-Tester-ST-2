#include "../lib/MeasuredResult.h"
#include "screensCommon.h"

int16_t drawMessageScreen(const char *title, int16_t optionsCount = 0, const char *options[] = {})
{
  int16_t startEncoderVal = AlexEncoder::counter;
  int8_t prevSelectedMenuItemIndex = 0;
  uint8_t xMargin = 35;
  uint8_t yMargin = 50;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;

  display->fillScreen(BLACK);
  drawStringHCentered(title, 25);

  for (int16_t i = 0; i < optionsCount; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));
    display->print(options[i]);
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  while (true)
  {
    // === ВЫХОД ДЛЯ API ===
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

    // -------------------------------
    // displayManager.drawMessageScreen(messageScreenId, useProgMem, title, options, optionsCount, resultOptionIndex);

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
    // -------------------------------

    if (button.isClicked())
    {
      return resultOptionIndex;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}