#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "lib/About.h"
#include "lib/SerialNumber.h"
#include "lib/AlexEncoder.h"
#include "lib/serialAPI.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

void drawAboutScreen()
{
  int16_t startEncoderVal = AlexEncoder::counter;
  uint8_t xMargin = 20;
  uint8_t yMargin = 105;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
  int8_t prevSelectedMenuItemIndex = -1;
  const uint8_t optionsCount = 2;
  const char *options[] = {"Go Back", "Rollback Firm. Update"};

  bool canRollback = false;
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition != NULL)
  {
    uint8_t dst;
    if (esp_partition_read(update_partition, 0, &dst, 1) == ESP_OK && dst == 0xE9)
    {
      canRollback = true;
    }
  }

  display->fillScreen(BLACK);

  while (true)
  {
    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

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
      display->fillScreen(BLACK);
      drawStringHCentered("About this device", 15);

      display->setCursor(10, 35);
      display->printf("Device model: %s", About::DEVICE_MODEL);
      display->setCursor(10, 50);
      display->printf("Hardware version: %s", About::HW_VERSION);
      display->setCursor(10, 65);
      display->printf("Firmware version: %s", About::SW_VERSION);
      display->setCursor(10, 80);
      display->printf("S/N: %s", SerialNumber::get());

      for (int16_t i = 0; i < optionsCount; i++)
      {
        if (i == 1 && !canRollback)
        {
          display->setTextColor(DARKGREY);
        }
        else
        {
          display->setTextColor(WHITE);
        }
        display->setCursor(xMargin, yMargin + (ySpacing * i));
        display->print(options[i]);
      }
      display->setTextColor(WHITE);

      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultOptionIndex));
      display->print("->");

      prevSelectedMenuItemIndex = resultOptionIndex;
    }

    if (button.isClicked())
    {
      if (resultOptionIndex == 0) // Go Back
      {
        return;
      }
      else if (resultOptionIndex == 1) // Rollback Firm. Update
      {
        if (!canRollback)
        {
          continue;
        }

        int16_t confirmStartEncoderVal = AlexEncoder::counter;
        int8_t confirmPrevSelectedIndex = -1;
        const uint8_t confirmOptionsCount = 2;
        const char *confirmOptions[] = {"No", "Yes"};
        bool returnToAbout = false;

        display->fillScreen(BLACK);

        while (true)
        {
          if (isApiRequestReceived)
            return; // ВЫХОД ДЛЯ API

          int16_t confirmIndex = AlexEncoder::counter - confirmStartEncoderVal;
          if (confirmIndex > confirmOptionsCount - 1)
          {
            confirmStartEncoderVal = AlexEncoder::counter - (confirmOptionsCount - 1);
            confirmIndex = confirmOptionsCount - 1;
          }
          if (confirmIndex < 0)
          {
            confirmIndex = 0;
            confirmStartEncoderVal = AlexEncoder::counter;
          }

          if (confirmPrevSelectedIndex != confirmIndex)
          {
            display->fillScreen(BLACK);
            drawStringHCentered("Rollback Firmware?", 15);

            display->setCursor(5, 35);
            display->print("Are you sure you want to");
            display->setCursor(5, 50);
            display->print("rollback? The device");
            display->setCursor(5, 65);
            display->print("will return to the");
            display->setCursor(5, 80);
            display->print("previous version.");

            for (int16_t i = 0; i < confirmOptionsCount; i++)
            {
              display->setCursor(xMargin, 110 + (ySpacing * i));
              display->print(confirmOptions[i]);
            }
            display->setCursor(xMargin - arrowXmargin, 110 + (ySpacing * confirmIndex));
            display->print("->");

            confirmPrevSelectedIndex = confirmIndex;
          }

          if (button.isClicked())
          {
            if (confirmIndex == 0) // No
            {
              returnToAbout = true;
              break;
            }
            else if (confirmIndex == 1) // Yes
            {
              display->fillScreen(BLACK);
              drawStringHCentered("Rolling back...", 60);

              if (update_partition != NULL)
              {
                esp_ota_set_boot_partition(update_partition);
              }

              display->fillScreen(BLACK);
              drawStringHCentered("Restart the device", 60);
              drawStringHCentered("to finish rollback", 80);

              while (true)
              {
                vTaskDelay(100 / portTICK_PERIOD_MS);
              }
            }
          }
        }

        if (returnToAbout)
        {
          prevSelectedMenuItemIndex = -1; // Force redraw of About menu
        }
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}