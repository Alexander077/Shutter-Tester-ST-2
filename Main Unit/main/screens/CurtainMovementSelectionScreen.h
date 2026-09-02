#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/MeasurementsCountSelectionScreen.h"
#include "screens/MeasuringScreen.h"
#include "screens/MeasuringSeriesScreen.h"
#include "lib/AlexEncoder.h"
#include "lib/serialAPI.h"

enum class CurtainMovementSelectionScreenResult
{
  GO_BACK,
  GO_TO_MAIN_MENU
};

const char *CurtainMovementItemsStr[CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT] =
{
  "Horisontal",
  "Vertical",
  "Leaf"
};

const char *GetCurtainMovementItemTitle(CurtainMovement menuItem)
{
  return CurtainMovementItemsStr[(uint8_t)menuItem];
}

CurtainMovementSelectionScreenResult drawCurtainMovementSelectionScreen()
{
  // Outer loop: redraw the screen when returning from the measurements count screen ('Go back')
  while (true)
  {
    display->fillScreen(BLACK);
    drawStringHCentered("Select curtain movement", 20);

    int16_t startEncoderVal = AlexEncoder::counter;
    uint8_t xMargin = 50;
    uint8_t yMargin = 55;
    uint8_t ySpacing = 15;
    uint8_t arrowXmargin = 15;
    const uint8_t totalMenuItemsCount = CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT + 1; //+1 for 'Go back' item

    for (uint8_t i = 0; i < totalMenuItemsCount; i++)
    {
      display->setCursor(xMargin, yMargin + (ySpacing * i));

      if (i == totalMenuItemsCount - 1)
      {
        display->println("Go Back");
      }
      else
      {
        display->println(GetCurtainMovementItemTitle((CurtainMovement)i));
      }
    }

    display->setCursor(xMargin - arrowXmargin, yMargin);
    display->print("->");

    int8_t resultMenuItemIndex = 0;
    int8_t prevSelectedMenuItemIndex = 0;

    while (true)
    {
      if (isApiRequestReceived)
        return CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU; // ВЫХОД ДЛЯ API

      int16_t resultMenuItemIndex = AlexEncoder::counter - startEncoderVal;

      if (resultMenuItemIndex > totalMenuItemsCount - 1)
      {
        startEncoderVal = AlexEncoder::counter - (totalMenuItemsCount - 1);
        resultMenuItemIndex = totalMenuItemsCount - 1;
      }

      if (resultMenuItemIndex < 0)
      {
        resultMenuItemIndex = 0;
        startEncoderVal = AlexEncoder::counter;
      }

      // displayManager.drawCurtainMovementSelectionScreen(resultMenuItemIndex);

      if (prevSelectedMenuItemIndex != resultMenuItemIndex)
      {
        display->setTextColor(BLACK);
        display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
        display->print("->");

        display->setTextColor(WHITE);
        display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultMenuItemIndex));
        display->print("->");

        prevSelectedMenuItemIndex = resultMenuItemIndex;
      }

      if (button.isClicked())
      {
        if (resultMenuItemIndex == totalMenuItemsCount - 1) //'Go back' selected
        {
          return CurtainMovementSelectionScreenResult::GO_BACK;
        }

        curtainMovement = (CurtainMovement)resultMenuItemIndex;

        uint8_t selectedMeasurementsCount = drawMeasurementsCountSelectionScreen();

        if (isApiRequestReceived)
          return CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU;

        if (selectedMeasurementsCount == 0) //'Go back' selected on measurements count screen
        {
          break; // break inner loop to redraw the curtain movement selection screen
        }

        if (selectedMeasurementsCount == 1)
        {
          drawMeasuringScreen();
        }
        else
        {
          drawMeasuringSeriesScreen(selectedMeasurementsCount);
        }

        return CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU;
      }

      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
  }
}