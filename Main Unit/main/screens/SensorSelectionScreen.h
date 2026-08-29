#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/MessageScreen.h"
#include "screens/CurtainMovementSelectionScreen.h"
#include "lib/serialAPI.h"
#include <inttypes.h>

inline void drawSensorSelectionScreen()
{
  const char *options[sensorsDataArraySize + 1] = {};

  for (size_t i = 0; i < sensorsDataArraySize; i++)
  {
    options[i] = SensorTypeStr[i];
  }

  options[sensorsDataArraySize] = "Go Back";

  while (true)
  {
    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

    int16_t selectedOptionIndex = drawMessageScreen("Select sensor type", sensorsDataArraySize + 1, options);
    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

    ESP_LOGI("", "Selected sensor index: %" PRIi16, selectedOptionIndex);

    if (selectedOptionIndex == sensorsDataArraySize) //'Go back' selected
    {
      return;
    }

    curSensorIndex = selectedOptionIndex;
    auto menuRes = drawCurtainMovementSelectionScreen();

    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

    if (menuRes == CurtainMovementSelectionScreenResult::GO_BACK)
    {
      continue;
    }

    if (menuRes == CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU)
    {
      return;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}