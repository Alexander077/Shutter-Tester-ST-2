#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "screens/MessageScreen.h"
#include "screens/MeasurementResultRecordSelectionScreen.h"
#include "screens/NameEditorScreen.h"
#include "lib/serialAPI.h"

enum class MeasurementSaveScreenResult
{
  OK,
  CANCEL
};

struct MeasurementRecordSaveResult
{
  bool isSuccess;
  int32_t newRecordNumber;
  char newRecordName[RECORD_NAME_BUF_SIZE];
};

const char *MeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ITEMS_COUNT] =
{
  "No",
  "Yes",
  "Overwrite oldest",
  "Overwrite newest",
  "Sel. rec. to overwrite",
  "Return to result",
};

const char *ZeroRecordsMeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT] =
{
  "No",
  "Yes",
  "Return to result",
};

int16_t getTotalMeasurementSaveSlotsCount()
{
  // return EEPROM_MEASURED_RES_TOTAL_BYTES / getMeasurementSaveRecordSize();
  return TOTAL_RECORDS_COUNT;
}

int16_t getFreeMeasurementSaveSlotsCount()
{
  return storage.getFreeSlotsCount();
}

MeasurementRecordSaveResult saveMesurementResult(const MeasuredResult &res, const char *name, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
{
  MeasurementRecordSaveResult saveRes = {false, -1, {0}};

  // Создаем структуру для сохранения и зануляем её память
  StoredMeasuredResult newRes;
  memset(&newRes, 0, sizeof(StoredMeasuredResult));

  // Если передали recordNumberToOverwrite, значит это режим перезаписи (update)
  // Иначе ставим 0, что для менеджера означает "создать новую запись или найти свободный слот"
  if (recordNumberToOverwrite != -1)
  {
    newRes.recordNumber = recordNumberToOverwrite;
  }
  else
  {
    newRes.recordNumber = 0;
  }

  newRes.isDeleted = false;

  if (name != NULL)
  {
    strncpy(newRes.name, name, RECORD_NAME_MAX_LEN);
    newRes.name[RECORD_NAME_MAX_LEN] = '\0';
  }

  // Аккуратно перекладываем все данные из текущего измерения
  newRes.sensor0Time = res.sensor0Time;
  newRes.sensor1Time = res.sensor1Time;
  newRes.curtain1spanAspeed = res.curtain1spanAspeed;
  newRes.curtain1spanAtime = res.curtain1spanAtime;
  newRes.curtain1TotalTime = res.curtain1TotalTime;
  newRes.curtain2spanAspeed = res.curtain2spanAspeed;
  newRes.curtain2spanAtime = res.curtain2spanAtime;
  newRes.curtain2TotalTime = res.curtain2TotalTime;
  newRes.slitWidthSensor0 = res.slitWidthSensor0;
  newRes.slitWidthSensor1 = res.slitWidthSensor1;
  newRes.slitWidthAverage = res.slitWidthAverage;
  newRes.usedSensorType = res.usedSensorType;
  newRes.selectedCurtainMovement = res.selectedCurtainMovement;

  // --- Обработка команд перезаписи самых новых или самых старых результатов ---
  if (overwriteOldest || overwriteNewest)
  {
    std::vector<int32_t> ids = storage.getAllValidRecordNumbers();

    if (!ids.empty())
    {
      // Находим минимальный (самый старый) и максимальный (самый новый) ID
      int32_t minId = ids[0];
      int32_t maxId = ids[0];

      for (int32_t id : ids)
      {
        if (id < minId)
          minId = id;
        if (id > maxId)
          maxId = id;
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
      // Если записей нет, но запрошена перезапись — просто сохраняем как новую
      newRes.recordNumber = 0;
    }
  }

  // Делегируем всю работу с файлом LittleFS и мьютексами классу-менеджеру
  int32_t savedId = storage.saveOrUpdateRecord(newRes);

  // Обработка результата
  if (savedId != -1)
  {
    saveRes.isSuccess = true;
    saveRes.newRecordNumber = savedId;
    strncpy(saveRes.newRecordName, newRes.name, RECORD_NAME_MAX_LEN);
    saveRes.newRecordName[RECORD_NAME_MAX_LEN] = '\0';
    ESP_LOGI("", "Meas. res. saved. Rec. num.: %" PRIi32 ", success: 1", savedId);
  }
  else
  {
    ESP_LOGE("", "Failed to save measurement result");
  }

  return saveRes;
}

// Duplicate check for the name editor: single-measurement store.
static bool isSingleRecordNameTaken(const char *name)
{
  return storage.isNameTaken(name);
}

// Duplicate check with one record excluded (rename flow).
static int32_t s_singleRenameExcludeNumber = -1;

static bool isSingleRecordNameTakenExcept(const char *name)
{
  return storage.isNameTaken(name, s_singleRenameExcludeNumber);
}

// Resolves the final record number for the overwrite modes (oldest/newest).
// Returns -1 when there is nothing to overwrite (caller falls back to Yes).
static int32_t resolveSingleOverwriteNumber(bool overwriteOldest, bool overwriteNewest)
{
  std::vector<int32_t> ids = storage.getAllValidRecordNumbers();

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

enum class SingleSaveNameChoice
{
  USE_NUMBER,
  CUSTOM,
  BACK
};

// "Save as 'NNNN'" / "Save as..." choice page. Returns BACK on API abort too;
// callers must check isApiRequestReceived right after the call.
static SingleSaveNameChoice drawSingleSaveNameChoiceScreen(int32_t previewNumber);

// Runs the name-choice page and, if needed, the name editor.
// On success fills outName and returns true. Returns false on Cancel/API.
static bool resolveSingleSaveName(int32_t previewNumber, char *outName);

// Saves with the "overwrite with number" semantics: the stored name is reset
// to the formatted target number (see user decision).
static MeasurementRecordSaveResult saveSingleWithNumberName(const MeasuredResult &res, int32_t targetNumber);

// Name-choice + save for one overwrite target. Returns false when the user
// cancelled the name step (caller returns to the save menu); on success fills
// saveRes and returns true.
static bool saveSingleOverwriteWithName(const MeasuredResult &res, int32_t targetNumber, int16_t menuIndex, bool &runLoop, int16_t &startEncoderVal, MeasurementRecordSaveResult &saveRes);

const char *GetSaveMasurementMenuItemTitle(MeasurementSaveMenuItems menuItem, bool useForZeroRecords)
{
  if (useForZeroRecords)
  {
    return ZeroRecordsMeasurementSaveItemsStr[(uint8_t)menuItem];
  }
  else
  {
    return MeasurementSaveItemsStr[(uint8_t)menuItem];
  }
}

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
        memset(&saveRes, 0, sizeof(saveRes));
        bool saveRequested = false;

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
              // Predict the number the new record will get (max + 1) so the
              // "Save as 'NNNN'" preview matches the stored name on success.
              int32_t previewNumber = 1;
              std::vector<int32_t> ids = storage.getAllValidRecordNumbers();
              for (size_t i = 0; i < ids.size(); i++)
              {
                if (ids[i] >= previewNumber)
                {
                  previewNumber = ids[i] + 1;
                }
              }

              char chosenName[RECORD_NAME_BUF_SIZE];
              memset(chosenName, 0, sizeof(chosenName));
              if (!resolveSingleSaveName(previewNumber, chosenName))
              {
                if (isApiRequestReceived)
                {
                  return MeasurementSaveScreenResult::CANCEL;
                }
                // Editor cancelled: back to the save menu.
                runLoop = false;
                startEncoderVal = AlexEncoder::counter - 1;
                continue;
              }

              ESP_LOGI("", "Saving record...");
              saveRes = saveMesurementResult(measurementRes, chosenName);
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
            int32_t targetNumber = resolveSingleOverwriteNumber(false, true);
            if (targetNumber == -1)
            {
              continue;
            }
            if (!saveSingleOverwriteWithName(measurementRes, targetNumber, (int16_t)resultIndex, runLoop, startEncoderVal, saveRes))
            {
              if (isApiRequestReceived)
              {
                return MeasurementSaveScreenResult::CANCEL;
              }
              continue;
            }
            saveRequested = true;
            break;
          }
          case MeasurementSaveMenuItems::OVERWRITE_OLDEST:
          {
            int32_t targetNumber = resolveSingleOverwriteNumber(true, false);
            if (targetNumber == -1)
            {
              continue;
            }
            if (!saveSingleOverwriteWithName(measurementRes, targetNumber, (int16_t)resultIndex, runLoop, startEncoderVal, saveRes))
            {
              if (isApiRequestReceived)
              {
                return MeasurementSaveScreenResult::CANCEL;
              }
              continue;
            }
            saveRequested = true;
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

            if (isApiRequestReceived)
            {
              return MeasurementSaveScreenResult::CANCEL;
            }

            if (!saveSingleOverwriteWithName(measurementRes, selectedMesurementRecordNumber, 4, runLoop, startEncoderVal, saveRes))
            {
              if (isApiRequestReceived)
              {
                return MeasurementSaveScreenResult::CANCEL;
              }
              continue;
            }
            saveRequested = true;
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
              char chosenName[RECORD_NAME_BUF_SIZE];
              memset(chosenName, 0, sizeof(chosenName));
              if (!resolveSingleSaveName(1, chosenName))
              {
                if (isApiRequestReceived)
                {
                  return MeasurementSaveScreenResult::CANCEL;
                }
                runLoop = false;
                startEncoderVal = AlexEncoder::counter - 1;
                continue;
              }

              ESP_LOGI("", "Saving record...");
              saveRes = saveMesurementResult(measurementRes, chosenName);
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
            return MeasurementSaveScreenResult::CANCEL;
            break;
          }
          default:
            halt("Halted. Unknown measurement save option");
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
          snprintf(resMessage, sizeof(resMessage), "Record saved as '%s'", saveRes.newRecordName);
#pragma GCC diagnostic pop
        }
        else
        {
          snprintf(resMessage, sizeof(resMessage), "Failed to save record");
        }
        const char *option[] = {"OK"};
        drawMessageScreen(resMessage, 1, option);
        return MeasurementSaveScreenResult::OK;
      }
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

static SingleSaveNameChoice drawSingleSaveNameChoiceScreen(int32_t previewNumber)
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
    return SingleSaveNameChoice::BACK;
  }

  if (choice == 0)
  {
    return SingleSaveNameChoice::USE_NUMBER;
  }
  return SingleSaveNameChoice::CUSTOM;
}

static bool resolveSingleSaveName(int32_t previewNumber, char *outName)
{
  SingleSaveNameChoice choice = drawSingleSaveNameChoiceScreen(previewNumber);

  if (isApiRequestReceived)
  {
    return false;
  }

  if (choice == SingleSaveNameChoice::USE_NUMBER)
  {
    formatRecordName(previewNumber, outName);
    return true;
  }

  NameEditorResult editorRes = drawNameEditorScreen("Enter name", "", isSingleRecordNameTaken);

  if (isApiRequestReceived || !editorRes.confirmed)
  {
    return false;
  }

  strncpy(outName, editorRes.name, RECORD_NAME_MAX_LEN);
  outName[RECORD_NAME_MAX_LEN] = '\0';
  return true;
}

static MeasurementRecordSaveResult saveSingleWithNumberName(const MeasuredResult &res, int32_t targetNumber)
{
  char numberedName[RECORD_NAME_BUF_SIZE];
  memset(numberedName, 0, sizeof(numberedName));
  formatRecordName(targetNumber, numberedName);
  return saveMesurementResult(res, numberedName, false, false, targetNumber);
}

static bool saveSingleOverwriteWithName(const MeasuredResult &res, int32_t targetNumber, int16_t menuIndex, bool &runLoop, int16_t &startEncoderVal, MeasurementRecordSaveResult &saveRes)
{
  char chosenName[RECORD_NAME_BUF_SIZE];
  memset(chosenName, 0, sizeof(chosenName));
  if (!resolveSingleSaveName(targetNumber, chosenName))
  {
    if (!isApiRequestReceived)
    {
      runLoop = false;
      startEncoderVal = AlexEncoder::counter - menuIndex;
    }
    return false;
  }

  ESP_LOGI("", "Saving record...");
  char numberedCheck[RECORD_NAME_BUF_SIZE];
  memset(numberedCheck, 0, sizeof(numberedCheck));
  formatRecordName(targetNumber, numberedCheck);
  if (strncmp(chosenName, numberedCheck, RECORD_NAME_BUF_SIZE) == 0)
  {
    saveRes = saveSingleWithNumberName(res, targetNumber);
  }
  else
  {
    saveRes = saveMesurementResult(res, chosenName, false, false, targetNumber);
  }
  return true;
}
