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

const char *MainMenuItemsStr[MAIN_MENU_ITEMS_COUNT] =
{
  "Measure",
  "Light Setup",
  "Saved Measures",
  "About"
};

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

// Draws (or erases) the selection markers around a main-menu item.
// color == WHITE draws "-> title <-"; color == BLACK erases only the markers.
// Only the two 3-character markers are touched - the centered title text is
// never re-rendered, so switching the selection no longer makes the item text
// blink (the previous implementation erased and re-printed the whole string).
static void drawMainMenuSelectionMarkers(
    int8_t menuItemIndex,
    const Rect menuItemsRects[MAIN_MENU_ITEMS_COUNT],
    const int16_t menuItemsY[MAIN_MENU_ITEMS_COUNT],
    uint16_t color)
{
  // u8g2_font_6x13_tf is monospace with a 6 px advance per character.
  const uint8_t fontCharWidth = 6;

  const Rect &r = menuItemsRects[menuItemIndex];
  int16_t y = menuItemsY[menuItemIndex];
  const char *title = GetMenuItemTitle((MainMenuItems)menuItemIndex);
  int16_t titleAdvancePx = (int16_t)strlen(title) * fontCharWidth;

  display->setTextColor(color);

  // "-> " to the left of the centered title
  display->setCursor(r.x - 3 * fontCharWidth, y);
  display->print("-> ");

  // " <-" to the right (the leading space keeps "<-" one cell away from the title) 
  display->setCursor(r.x + titleAdvancePx, y);
  display->print(" <-");

  display->setTextColor(WHITE);
}

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
          drawMainMenuSelectionMarkers(prevSelectedMenuItemIndex, menuItemsRects, menuItemsY, BLACK);
        }

        drawMainMenuSelectionMarkers(resultMenuItemIndex, menuItemsRects, menuItemsY, WHITE);
        prevSelectedMenuItemIndex = resultMenuItemIndex;
      }

      // === ПЕРЕХВАТЧИК API ===
      if (isApiRequestReceived)
      {
        isApiRequestReceived = false;
        ApiRequestAction dispatchedAction = apiRequestAction;

        switch (dispatchedAction)
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

        // Reset the handled action so it does not leak into later manual flows
        // (the measured screen checks GO_TO_MEASURE to emit the API JSON).
        // If a newer request arrived while the screen was active, keep it.
        if (!isApiRequestReceived && apiRequestAction == dispatchedAction)
        {
          apiRequestAction = ApiRequestAction::NO_ACTION;
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