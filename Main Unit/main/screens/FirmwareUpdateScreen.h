#pragma once

#include "screensCommon.h"
#include "lib/SerialAPICommon.h"

void drawFirmwareUpdateScreen()
{
  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);

  drawStringHCentered("Firmware update", 45);
  drawStringHCentered("Do not disconnect", 70);
  drawStringHCentered("the device", 85);

  while (isFirmwareUpdateInProgress)
  {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
