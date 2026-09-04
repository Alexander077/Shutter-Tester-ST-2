#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "screens/MessageScreen.h"
#include "screens/MeasurementResultRecordSelectionScreen.h"
#include "lib/serialAPI.h"
#include <vector>

enum class SeriesSaveScreenResult
{
  OK,
  CANCEL
};

struct SeriesRecordSaveResult
{
  bool isSuccess;
  int32_t newRecordNumber;
};

const char *SeriesSaveItemsStr[SAVE_MEASUREMENT_MENU_ITEMS_COUNT] =
{
  "No",
  "Yes",
  "Overwrite oldest",
  "Overwrite newest",
  "Sel. rec. to overwrite",
  "Return to report",
};

const char *ZeroRecordsSeriesSaveItemsStr[SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT] =
{
  "No",
  "Yes",
  "Return to report",
};

int16_t getTotalSeriesSaveSlotsCount()
{
  return TOTAL_SERIES_COUNT;
}

int16_t getFreeSeriesSaveSlotsCount()
{
  return seriesStorage.getFreeSlotsCount();
}

SeriesRecordSaveResult saveSeriesResult(const MeasurementSeries &series, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
{
  SeriesRecordSaveResult saveRes = {false, -1};

  StoredMeasuredSeries newRes;
  memset(&newRes, 0, sizeof(StoredMeasuredSeries));

  if (recordNumberToOverwrite != -1)
  {
    newRes.recordNumber = recordNumberToOverwrite;
  }
  else
  {
    newRes.recordNumber = 0;
  }

  newRes.isDeleted = false;
  newRes.measurementCount = series.storedCount;

  for (uint8_t i = 0; i < series.storedCount && i < MEASUREMENT_SERIES_MAX_SIZE; i++)
  {
    newRes.results[i] = series.results[i];
  }

  if (overwriteOldest || overwriteNewest)
  {
    std::vector<int32_t> ids = seriesStorage.getAllValidRecordNumbers();

    if (!ids.empty())
    {
      int32_t minId = ids[0];
      int32_t maxId = ids[0];

      for (int32_t id : ids)
      {
        if (id < minId) minId = id;
        if (id > maxId) maxId = id;
      }

      if (overwriteOldest)
      {
        newRes.recordNumber = minId;
      }
      else if (overwriteNewest)
      {
        newRes.recordNumber = maxId;
      }
    }
    else
    {
      newRes.recordNumber = 0;
    }
  }

  int32_t savedId = seriesStorage.saveOrUpdateRecord(newRes);

  if (savedId != -1)
  {
    saveRes.isSuccess = true;
    saveRes.newRecordNumber = savedId;
    ESP_LOGI("", "Series saved. Rec. num.: %" PRIi32, savedId);
  }
  else
  {
    ESP_LOGE("", "Failed to save series result");
  }

  return saveRes;
}

const char *GetSaveSeriesMenuItemTitle(MeasurementSaveMenuItems menuItem, bool useForZeroRecords)
{
  if (useForZeroRecords)
  {
    return ZeroRecordsSeriesSaveItemsStr[(uint8_t)menuItem];
  }
  else
  {
    return SeriesSaveItemsStr[(uint8_t)menuItem];
  }
}

SeriesSaveScreenResult drawSeriesSaveScreen(MeasurementSeries &measurementSeries)
{
  int16_t freeSlotsCount = getFreeSeriesSaveSlotsCount();
  int16_t totalRecordsCount = getTotalSeriesSaveSlotsCount();
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
    drawStringHCentered("Save series result?", 20);

    for (uint8_t i = 0; i < menuItemsCount; i++)
    {
      display->setCursor(xMargin, yMargin + (ySpacing * i));

      if (i == (uint8_t)MeasurementSaveMenuItems::YES)
      {
        if (freeSlotsCount == 0)
        {
          display->setTextColor(DARKGREY);
          display->printf("%s(no free slots)", GetSaveSeriesMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
          display->setTextColor(WHITE);
        }
        else
        {
          display->printf("%s(%i free slots)", GetSaveSeriesMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems), freeSlotsCount);
        }
      }
      else
      {
        display->println(GetSaveSeriesMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
      }
    }

    display->setCursor(xMargin - arrowXmargin, yMargin);
    display->print("->");

    int16_t prevSelectedMenuItemIndex = 0;
    bool runLoop = true;

    while (runLoop)
    {
      if (isApiRequestReceived)
        return SeriesSaveScreenResult::CANCEL;

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

      if (button.isClicked())
      {
        SeriesRecordSaveResult saveRes = {false, -1};

        if (drawMoreThanZeroRecordsMenuItems)
        {
          switch ((MeasurementSaveMenuItems)resultIndex)
          {
          case MeasurementSaveMenuItems::NO:
          {
            return SeriesSaveScreenResult::OK; // do not save the result
            break;
          }
          case MeasurementSaveMenuItems::YES:
          {
            if (freeSlotsCount > 0)
            {
              ESP_LOGI("", "Saving series...");
              saveRes = saveSeriesResult(measurementSeries);
            }
            else
            {
              continue;
            }

            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_NEWEST:
          {
            ESP_LOGI("", "Saving series...");
            saveRes = saveSeriesResult(measurementSeries, false, true);
            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_OLDEST:
          {
            ESP_LOGI("", "Saving series...");
            saveRes = saveSeriesResult(measurementSeries, true);
            break;
          }
          case MeasurementSaveMenuItems::CHOOSE_RECORD_TO_OVERWRITE:
          {
            int32_t selectedSeriesRecordNumber = drawSeriesRecordSelectionScreen();

            if (selectedSeriesRecordNumber == 0) // user selected "Back" option
            {
              runLoop = false;
              startEncoderVal = AlexEncoder::counter - 4; // Point to same menu item
              continue;
            }

            ESP_LOGI("", "Saving series...");
            saveRes = saveSeriesResult(measurementSeries, false, false, selectedSeriesRecordNumber);
            break;
          }
          case MeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
          {
            return SeriesSaveScreenResult::CANCEL;
            break;
          }
          default:
            halt("Halted. Unknown series save option");
            break;
          }
        }
        else
        {
          switch ((ZeroRecordsMeasurementSaveMenuItems)resultIndex)
          {
          case ZeroRecordsMeasurementSaveMenuItems::NO:
          {
            return SeriesSaveScreenResult::OK; // do not save the result
            break;
          }
          case ZeroRecordsMeasurementSaveMenuItems::YES:
          {
            if (freeSlotsCount > 0)
            {
              ESP_LOGI("", "Saving series...");
              saveRes = saveSeriesResult(measurementSeries);
            }
            else
            {
              continue;
            }

            break;
          }
          case ZeroRecordsMeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
          {
            return SeriesSaveScreenResult::CANCEL;
            break;
          }
          default:
            halt("Halted. Unknown series save option");
            break;
          }
        }

        char resMessage[50];

        if (saveRes.isSuccess)
        {
          char buf[30];
          formatRecordName(saveRes.newRecordNumber, buf);
#pragma GCC diagnostic ignored "-Wformat"
          sprintf(resMessage, "Series saved as '%s'", buf);
#pragma GCC diagnostic pop
        }
        else
        {
          sprintf(resMessage, "Failed to save series");
        }
        const char *option[] = {"OK"};
        drawMessageScreen(resMessage, 1, option);
        return SeriesSaveScreenResult::OK;
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}


