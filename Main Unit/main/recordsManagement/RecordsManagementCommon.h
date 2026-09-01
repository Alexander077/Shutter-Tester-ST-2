#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_littlefs.h"
#include "lib/Common.h"
#include "lib/MeasuredResult.h"
#include "recordsManagement/StoredMeasuredResult.h"
#include "recordsManagement/RecordsStorageManager.h"

#define TOTAL_RECORDS_COUNT 100
#define LITTLEFS_PARTITION_LABEL "storage"
#define LITTLEFS_PARTITION_NAME "littlefs"
#define LITTLEFS_BASE_PATH "/" LITTLEFS_PARTITION_NAME
#define RECORDS_FILE_NAME "records.bin"
#define RECORDS_FILE_PATH LITTLEFS_BASE_PATH "/" RECORDS_FILE_NAME

RecordsStorageManager storage(RECORDS_FILE_PATH);

struct MeasurementRecordSaveResult
{
  bool isSuccess;
  int32_t newRecordNumber;
};

int16_t getMeasurementSaveRecordSize();

void initStorage()
{
  ESP_LOGI("", "Initializing LittleFS");

  esp_vfs_littlefs_conf_t conf = {
      .base_path = LITTLEFS_BASE_PATH,
      .partition_label = LITTLEFS_PARTITION_LABEL,
      .format_if_mount_failed = true,
      .dont_mount = false,
  };

  esp_err_t ret = esp_vfs_littlefs_register(&conf);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      halt("Failed to mount or format filesystem");
    }
    else if (ret == ESP_ERR_NOT_FOUND)
    {
      halt("Failed to find LittleFS partition");
    }
    else
    {
      ESP_LOGE("", "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
      halt();
    }
    return;
  }

  size_t total = 0, used = 0;
  ret = esp_littlefs_info(conf.partition_label, &total, &used);

  if (ret != ESP_OK)
  {
    ESP_LOGE("", "Failed to get LittleFS partition information: (%s). Formatting...", esp_err_to_name(ret));
    esp_littlefs_format(conf.partition_label);
  }
  else
  {
    ESP_LOGI("", "Storage partition size: total: %d, used: %d", total, used);
  }

  // esp_vfs_littlefs_unregister(conf.partition_label);
  // ESP_LOGI(TAG, "LittleFS unmounted");

  struct stat st;
  int16_t blockSize = getMeasurementSaveRecordSize();
  uint8_t data[blockSize] = {};

  if (stat(RECORDS_FILE_PATH, &st) != 0) // if no records.bin file found
  {
    ESP_LOGI("", "No '" RECORDS_FILE_NAME "' file found. Initializing...");

    FILE *recordsFile = fopen(RECORDS_FILE_PATH, "w");

    for (int32_t i = 0; i < TOTAL_RECORDS_COUNT; i++)
    {
      size_t res = fwrite(data, blockSize, 1, recordsFile);

      if (res == 0)
      {
        halt("Halted. Failed to write inital data to '" RECORDS_FILE_NAME "'");
      }
    }

    ESP_LOGI("", "'" RECORDS_FILE_NAME "' initialized");
    fclose(recordsFile);

    ESP_LOGI("", "Verifing writen data...");

    recordsFile = fopen(RECORDS_FILE_PATH, "r");

    uint8_t buf[1] = {};

    for (int32_t i = 0; i < TOTAL_RECORDS_COUNT * blockSize; i++)
    {
      fread(buf, 1, 1, recordsFile);

      if (buf[0] != 0)
      {
        halt("Invalid data");
      }
    }

    fclose(recordsFile);
    ESP_LOGI("", "Data OK");
  }
}

int16_t getMeasurementSaveRecordSize()
{
  return sizeof(StoredMeasuredResult);
}

int16_t getTotalMeasurementSaveSlotsCount()
{
  // return EEPROM_MEASURED_RES_TOTAL_BYTES / getMeasurementSaveRecordSize();
  return TOTAL_RECORDS_COUNT;
}

int16_t getFreeMeasurementSaveSlotsCount()
{
  return storage.getFreeSlotsCount();
}

StoredMeasuredResult getMeasurementStoredResultByNumber(int32_t recordNumber)
{
  return storage.getRecordByNumber(recordNumber);
}

bool deleteMeasurementStoredResultByNumber(int32_t recordNumber)
{
  return storage.deleteRecordByNumber(recordNumber);
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