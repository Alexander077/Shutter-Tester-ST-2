#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "screens/MessageScreen.h"
#include "screens/MeasurementResultRecordSelectionScreen.h"
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

MeasurementRecordSaveResult saveMesurementResult(const MeasuredResult &res, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
{
  MeasurementRecordSaveResult saveRes = {false, -1};

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
    ESP_LOGI("", "Meas. res. saved. Rec. num.: %" PRIi32 ", success: 1", savedId);
  }
  else
  {
    ESP_LOGE("", "Failed to save measurement result");
  }

  return saveRes;
}

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