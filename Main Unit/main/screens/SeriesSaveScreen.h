#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "screens/MessageScreen.h"
#include "screens/MeasurementResultRecordSelectionScreen.h"
#include "screens/NameEditorScreen.h"
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
  char newRecordName[RECORD_NAME_BUF_SIZE];
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

SeriesRecordSaveResult saveSeriesResult(const MeasurementSeries &series, const char *name, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
{
  SeriesRecordSaveResult saveRes = {false, -1, {0}};

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

  if (name != NULL)
  {
    strncpy(newRes.name, name, RECORD_NAME_MAX_LEN);
    newRes.name[RECORD_NAME_MAX_LEN] = '\0';
  }

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
    strncpy(saveRes.newRecordName, newRes.name, RECORD_NAME_MAX_LEN);
    saveRes.newRecordName[RECORD_NAME_MAX_LEN] = '\0';
    ESP_LOGI("", "Series saved. Rec. num.: %" PRIi32, savedId);
  }
  else
  {
    ESP_LOGE("", "Failed to save series result");
  }

  return saveRes;
}

// Duplicate check for the name editor: series store.
static bool isSeriesRecordNameTaken(const char *name)
{
  return seriesStorage.isNameTaken(name);
}

// Duplicate check with one record excluded (rename flow).
static int32_t s_seriesRenameExcludeNumber = -1;

static bool isSeriesRecordNameTakenExcept(const char *name)
{
  return seriesStorage.isNameTaken(name, s_seriesRenameExcludeNumber);
}

static int32_t resolveSeriesOverwriteNumber(bool overwriteOldest, bool overwriteNewest)
{
  std::vector<int32_t> ids = seriesStorage.getAllValidRecordNumbers();

  if (ids.empty())
  {
    return -1;
  }

  int32_t minId = ids[0];
  int32_t maxId = ids[0];

  for (size_t i = 0; i < ids.size(); i++)
  {
    if (ids[i] < minId)
    {
      minId = ids[i];
    }
    if (ids[i] > maxId)
    {
      maxId = ids[i];
    }
  }

  if (overwriteOldest)
  {
    return minId;
  }
  return maxId;
}

static int32_t predictSeriesPreviewNumber()
{
  int32_t previewNumber = 1;
  std::vector<int32_t> ids = seriesStorage.getAllValidRecordNumbers();
  for (size_t i = 0; i < ids.size(); i++)
  {
    if (ids[i] >= previewNumber)
    {
      previewNumber = ids[i] + 1;
    }
  }
  return previewNumber;
}

enum class SeriesSaveNameChoice
{
  USE_NUMBER,
  CUSTOM,
  BACK
};

static SeriesSaveNameChoice drawSeriesSaveNameChoiceScreen(int32_t previewNumber);

static bool resolveSeriesSaveName(int32_t previewNumber, char *outName);

static SeriesRecordSaveResult saveSeriesWithNumberName(const MeasurementSeries &series, int32_t targetNumber);

static bool saveSeriesOverwriteWithName(const MeasurementSeries &series, int32_t targetNumber, int16_t menuIndex, bool &runLoop, int16_t &startEncoderVal, SeriesRecordSaveResult &saveRes);

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
        SeriesRecordSaveResult saveRes = {false, -1, {0}};
        bool saveRequested = false;

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
              char chosenName[RECORD_NAME_BUF_SIZE];
              memset(chosenName, 0, sizeof(chosenName));
              if (!resolveSeriesSaveName(predictSeriesPreviewNumber(), chosenName))
              {
                if (isApiRequestReceived)
                {
                  return SeriesSaveScreenResult::CANCEL;
                }
                runLoop = false;
                startEncoderVal = AlexEncoder::counter - 1;
                continue;
              }

              ESP_LOGI("", "Saving series...");
              saveRes = saveSeriesResult(measurementSeries, chosenName);
              saveRequested = true;
            }
            else
            {
              continue;
            }

            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_NEWEST:
          {
            int32_t targetNumber = resolveSeriesOverwriteNumber(false, true);
            if (targetNumber == -1)
            {
              continue;
            }
            if (!saveSeriesOverwriteWithName(measurementSeries, targetNumber, (int16_t)resultIndex, runLoop, startEncoderVal, saveRes))
            {
              if (isApiRequestReceived)
              {
                return SeriesSaveScreenResult::CANCEL;
              }
              continue;
            }
            saveRequested = true;
            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_OLDEST:
          {
            int32_t targetNumber = resolveSeriesOverwriteNumber(true, false);
            if (targetNumber == -1)
            {
              continue;
            }
            if (!saveSeriesOverwriteWithName(measurementSeries, targetNumber, (int16_t)resultIndex, runLoop, startEncoderVal, saveRes))
            {
              if (isApiRequestReceived)
              {
                return SeriesSaveScreenResult::CANCEL;
              }
              continue;
            }
            saveRequested = true;
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

            if (isApiRequestReceived)
            {
              return SeriesSaveScreenResult::CANCEL;
            }

            if (!saveSeriesOverwriteWithName(measurementSeries, selectedSeriesRecordNumber, 4, runLoop, startEncoderVal, saveRes))
            {
              if (isApiRequestReceived)
              {
                return SeriesSaveScreenResult::CANCEL;
              }
              continue;
            }
            saveRequested = true;
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
              char chosenName[RECORD_NAME_BUF_SIZE];
              memset(chosenName, 0, sizeof(chosenName));
              if (!resolveSeriesSaveName(1, chosenName))
              {
                if (isApiRequestReceived)
                {
                  return SeriesSaveScreenResult::CANCEL;
                }
                runLoop = false;
                startEncoderVal = AlexEncoder::counter - 1;
                continue;
              }

              ESP_LOGI("", "Saving series...");
              saveRes = saveSeriesResult(measurementSeries, chosenName);
              saveRequested = true;
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

        if (!saveRequested)
        {
          continue;
        }

        char resMessage[RECORD_NAME_BUF_SIZE + 24];

        if (saveRes.isSuccess)
        {
#pragma GCC diagnostic ignored "-Wformat"
          snprintf(resMessage, sizeof(resMessage), "Series saved as '%s'", saveRes.newRecordName);
#pragma GCC diagnostic pop
        }
        else
        {
          snprintf(resMessage, sizeof(resMessage), "Failed to save series");
        }
        const char *option[] = {"OK"};
        drawMessageScreen(resMessage, 1, option);
        return SeriesSaveScreenResult::OK;
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

static SeriesSaveNameChoice drawSeriesSaveNameChoiceScreen(int32_t previewNumber)
{
  char numberedOption[RECORD_NAME_BUF_SIZE + 12];
  char numBuf[30];
  formatRecordName(previewNumber, numBuf);
#pragma GCC diagnostic ignored "-Wformat"
  snprintf(numberedOption, sizeof(numberedOption), "Save as '%s'", numBuf);
#pragma GCC diagnostic pop
  const char *customOption = "Save as...";

  const char *options[] = {numberedOption, customOption};
  int16_t choice = drawMessageScreen("Choose name", 2, options);

  if (isApiRequestReceived)
  {
    return SeriesSaveNameChoice::BACK;
  }

  if (choice == 0)
  {
    return SeriesSaveNameChoice::USE_NUMBER;
  }
  return SeriesSaveNameChoice::CUSTOM;
}

static bool resolveSeriesSaveName(int32_t previewNumber, char *outName)
{
  SeriesSaveNameChoice choice = drawSeriesSaveNameChoiceScreen(previewNumber);

  if (isApiRequestReceived)
  {
    return false;
  }

  if (choice == SeriesSaveNameChoice::USE_NUMBER)
  {
    formatRecordName(previewNumber, outName);
    return true;
  }

  NameEditorResult editorRes = drawNameEditorScreen("Enter name", "", isSeriesRecordNameTaken);

  if (isApiRequestReceived || !editorRes.confirmed)
  {
    return false;
  }

  strncpy(outName, editorRes.name, RECORD_NAME_MAX_LEN);
  outName[RECORD_NAME_MAX_LEN] = '\0';
  return true;
}

static SeriesRecordSaveResult saveSeriesWithNumberName(const MeasurementSeries &series, int32_t targetNumber)
{
  char numberedName[RECORD_NAME_BUF_SIZE];
  memset(numberedName, 0, sizeof(numberedName));
  formatRecordName(targetNumber, numberedName);
  return saveSeriesResult(series, numberedName, false, false, targetNumber);
}

static bool saveSeriesOverwriteWithName(const MeasurementSeries &series, int32_t targetNumber, int16_t menuIndex, bool &runLoop, int16_t &startEncoderVal, SeriesRecordSaveResult &saveRes)
{
  char chosenName[RECORD_NAME_BUF_SIZE];
  memset(chosenName, 0, sizeof(chosenName));
  if (!resolveSeriesSaveName(targetNumber, chosenName))
  {
    if (!isApiRequestReceived)
    {
      runLoop = false;
      startEncoderVal = AlexEncoder::counter - menuIndex;
    }
    return false;
  }

  ESP_LOGI("", "Saving series...");
  char numberedCheck[RECORD_NAME_BUF_SIZE];
  memset(numberedCheck, 0, sizeof(numberedCheck));
  formatRecordName(targetNumber, numberedCheck);
  if (strncmp(chosenName, numberedCheck, RECORD_NAME_BUF_SIZE) == 0)
  {
    saveRes = saveSeriesWithNumberName(series, targetNumber);
  }
  else
  {
    saveRes = saveSeriesResult(series, chosenName, false, false, targetNumber);
  }
  return true;
}



