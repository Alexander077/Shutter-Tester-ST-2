#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "screens/MessageScreen.h"
#include "screens/MeasurementResultRecordSelectionScreen.h"
#include "lib/serialAPI.h"

MeasurementSaveScreenResult drawMeasurementSaveScreen(MeasuredResult &measurementRes)
{
  int16_t freeSlotsCount = getFreeMeasurementSaveSlotsCount();
  int16_t totalRecordsCount = getTotalMeasurementSaveSlotsCount();
  int16_t menuItemsCount = totalRecordsCount == freeSlotsCount ? SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT : SAVE_MEASUREMENT_MENU_ITEMS_COUNT;
  int16_t startEncoderVal = AlexEncoder::counter;
  bool drawMoreThanZeroRecordsMenuItems = menuItemsCount == SAVE_MEASUREMENT_MENU_ITEMS_COUNT;

  uint8_t xMargin = 20;
  uint8_t yMargin = 40;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;

  while (true)
  {
    display->fillScreen(BLACK);
    drawStringHCentered("Save measurement result?", 20);

    for (uint8_t i = 0; i < menuItemsCount; i++)
    {
      display->setCursor(xMargin, yMargin + (ySpacing * i));

      if (i == (uint8_t)MeasurementSaveMenuItems::YES)
      {
        if (freeSlotsCount == 0)
        {
          display->setTextColor(DARKGREY);
          display->printf("%s(no free slots)", GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
          display->setTextColor(WHITE);
        }
        else
        {
          display->printf("%s(%i free slots)", GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems), freeSlotsCount);
        }
      }
      else
      {
        display->println(GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
      }
    }

    display->setCursor(xMargin - arrowXmargin, yMargin);
    display->print("->");

    int16_t prevSelectedMenuItemIndex = 0;
    bool runLoop = true;

    while (runLoop)
    {
      // === ВЫХОД ДЛЯ API ===
      if (isApiRequestReceived)
        return MeasurementSaveScreenResult::CANCEL;

      int16_t resultIndex = AlexEncoder::counter - startEncoderVal;

      if (resultIndex > menuItemsCount - 1)
      {
        startEncoderVal = AlexEncoder::counter - (menuItemsCount - 1);
        resultIndex = menuItemsCount - 1;
      }

      if (resultIndex < 0)
      {
        resultIndex = 0;
        startEncoderVal = AlexEncoder::counter;
      }

      //-------------------------
      // displayManager.drawMeasurementSaveScreen(resultIndex, freeSlotsCount, drawMoreThanZeroRecordsMenuItems);

      if (prevSelectedMenuItemIndex != resultIndex)
      {
        display->setTextColor(BLACK);
        display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
        display->print("->");

        display->setTextColor(WHITE);
        display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultIndex));
        display->print("->");

        prevSelectedMenuItemIndex = resultIndex;
      }

      //-----------------------------

      if (button.isClicked())
      {
        MeasurementRecordSaveResult saveRes;

        if (drawMoreThanZeroRecordsMenuItems)
        {
          switch ((MeasurementSaveMenuItems)resultIndex)
          {
          case MeasurementSaveMenuItems::NO:
          {
            return MeasurementSaveScreenResult::OK; // do not save the result
            break;
          }
          case MeasurementSaveMenuItems::YES:
          {
            if (freeSlotsCount > 0)
            {
              ESP_LOGI("", "Saving record...");
              saveRes = saveMesurementResult(measurementRes);
            }
            else
            {
              continue;
            }

            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_NEWEST:
          {
            ESP_LOGI("", "Saving record...");
            saveRes = saveMesurementResult(measurementRes, false, true);
            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_OLDEST:
          {
            ESP_LOGI("", "Saving record...");
            saveRes = saveMesurementResult(measurementRes, true);
            break;
          }
          case MeasurementSaveMenuItems::CHOOSE_RECORD_TO_OVERWRITE:
          {
            int32_t selectedMesurementRecordNumber = drawMeasurementResultRecordSelectionScreen();

            if (selectedMesurementRecordNumber == 0) // user selected "Back" option
            {
              runLoop = false;
              startEncoderVal = AlexEncoder::counter - 4; // Point to same menu item
              continue;
            }

            ESP_LOGI("", "Saving record...");
            saveRes = saveMesurementResult(measurementRes, false, false, selectedMesurementRecordNumber);
            break;
          }
          case MeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
          {
            return MeasurementSaveScreenResult::CANCEL;
            break;
          }
          default:
            halt("Halted. Unknown measurement save option");
            break;
          }
        }
        else
        {
          switch ((ZeroRecordsMeasurementSaveMenuItems)resultIndex)
          {
          case ZeroRecordsMeasurementSaveMenuItems::NO:
          {
            return MeasurementSaveScreenResult::OK; // do not save the result
            break;
          }
          case ZeroRecordsMeasurementSaveMenuItems::YES:
          {
            if (freeSlotsCount > 0)
            {
              ESP_LOGI("", "Saving record...");
              saveRes = saveMesurementResult(measurementRes);
            }
            else
            {
              continue;
            }

            break;
          }
          case ZeroRecordsMeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
          {
            return MeasurementSaveScreenResult::CANCEL;
            break;
          }
          default:
            halt("Halted. Unknown measurement save option");
            break;
          }
        }

        char resMessage[50];

        if (saveRes.isSuccess)
        {
          char buf[30];
          formatRecordName(saveRes.newRecordNumber, buf);
#pragma GCC diagnostic ignored "-Wformat"
          sprintf(resMessage, "Record saved as '%s'", buf);
#pragma GCC diagnostic pop
        }
        else
        {
          sprintf(resMessage, "Failed to save record");
        }
        const char *option[] = {"OK"};
        drawMessageScreen(resMessage, 1, option);
        return MeasurementSaveScreenResult::OK;
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}