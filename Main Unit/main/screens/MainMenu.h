#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/SensorSelectionScreen.h"
#include "screens/LightCheckScreen.h"
#include "screens/AboutScreen.h"
#include "screens/ViewRecordsScreen.h"
#include "screens/MeasuringScreen.h"
#include "lib/AlexEncoder.h"
#include "lib/serialAPI.h"

void drawMainMenu()
{
  int16_t startEncoderVal = AlexEncoder::counter;
  Rect menuItemsRects[MAIN_MENU_ITEMS_COUNT];
  int16_t menuItemsY[MAIN_MENU_ITEMS_COUNT];

  while (true)
  {
    display->fillScreen(BLACK);

    for (uint8_t i = 0, y = 45; i < MAIN_MENU_ITEMS_COUNT; i++, y += 15)
    {
      menuItemsRects[i] = drawStringHCentered(GetMenuItemTitle((MainMenuItems)i), y);
      menuItemsY[i] = y;
    }

    int8_t resultMenuItemIndex = 0;
    int8_t prevSelectedMenuItemIndex = -1;

    while (true)
    {
      resultMenuItemIndex = AlexEncoder::counter - startEncoderVal;

      if (resultMenuItemIndex > MAIN_MENU_ITEMS_COUNT - 1)
      {
        startEncoderVal = AlexEncoder::counter - (MAIN_MENU_ITEMS_COUNT - 1);
        resultMenuItemIndex = MAIN_MENU_ITEMS_COUNT - 1;
      }

      if (resultMenuItemIndex < 0)
      {
        resultMenuItemIndex = 0;
        startEncoderVal = AlexEncoder::counter;
      }

      // displayManager.drawMainMenu(resultMenuItemIndex);
      if (prevSelectedMenuItemIndex != resultMenuItemIndex)
      {
        if (prevSelectedMenuItemIndex != -1)
        {
          Rect rOld = menuItemsRects[prevSelectedMenuItemIndex];
          display->fillRect(rOld.x, rOld.y /* - rOld.height */, rOld.width, rOld.height, BLACK);
          menuItemsRects[prevSelectedMenuItemIndex] =
              drawStringHCentered(GetMenuItemTitle((MainMenuItems)prevSelectedMenuItemIndex), menuItemsY[prevSelectedMenuItemIndex]);
        }

        String newSelMenuItem = "-> " + String(GetMenuItemTitle((MainMenuItems)resultMenuItemIndex)) + " <-";
        Rect rNew = menuItemsRects[resultMenuItemIndex];
        display->fillRect(rNew.x, rNew.y /* - rNew.height */, rNew.width, rNew.height, BLACK);

        menuItemsRects[resultMenuItemIndex] = drawStringHCentered(newSelMenuItem, menuItemsY[resultMenuItemIndex]);
        prevSelectedMenuItemIndex = resultMenuItemIndex;
      }

      // === ПЕРЕХВАТЧИК API ===
      if (isApiRequestReceived)
      {
        isApiRequestReceived = false;

        switch (apiRequestAction)
        {
        case ApiRequestAction::GO_TO_LIGHT_SETUP:
          drawLightCheckScreen();
          startEncoderVal = AlexEncoder::counter;
          break;

        case ApiRequestAction::GO_TO_MEASURE:
          drawMeasuringScreen();
          startEncoderVal = AlexEncoder::counter;
          break;

        default:
          break;
        }

        break; // break inner while to redraw menu with correct item selected after API action
      }
      // =======================

      if (button.isClicked())
      {
        switch ((MainMenuItems)resultMenuItemIndex)
        {
        case MainMenuItems::MEASURE:
        {
          // drawCurtainMovementSelectionScreen();
          drawSensorSelectionScreen();
          break;
        }
        case MainMenuItems::CHECK_LIGHT:
        {
          drawLightCheckScreen();
          break;
        }
        case MainMenuItems::CREDITS:
        {
          drawAboutScreen();
          break;
        }
        case MainMenuItems::MEASURMENT_HISTORY:
        {
          drawViewRecordsScreen();
          break;
        }

        default:
          halt("Halted. Unknown menu item");
          break;
        }

        startEncoderVal = AlexEncoder::counter;
        break; // break from while loop
      }

      vTaskDelay(1 / portTICK_PERIOD_MS);
    }
  }
}