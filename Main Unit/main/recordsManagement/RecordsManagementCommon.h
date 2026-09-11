#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <vector>
#include <sys/stat.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_littlefs.h"
#include "lib/Common.h"
#include "lib/MeasuredResult.h"
#include "recordsManagement/StoredMeasuredResult.h"
#include "recordsManagement/StoredMeasuredSeries.h"
#include "recordsManagement/LegacyRecordFormats.h"
#include "recordsManagement/RecordsStorageManager.h"

#define TOTAL_RECORDS_COUNT 100
#define TOTAL_SERIES_COUNT 100
#define LITTLEFS_PARTITION_LABEL "storage"
#define LITTLEFS_PARTITION_NAME "littlefs"
#define LITTLEFS_BASE_PATH "/" LITTLEFS_PARTITION_NAME
#define RECORDS_FILE_NAME "records.bin"
#define RECORDS_FILE_PATH LITTLEFS_BASE_PATH "/" RECORDS_FILE_NAME
#define SERIES_FILE_NAME "seriesRecords.bin"
#define SERIES_FILE_PATH LITTLEFS_BASE_PATH "/" SERIES_FILE_NAME

RecordsStorageManager storage(RECORDS_FILE_PATH);
SeriesStorageManager seriesStorage(SERIES_FILE_PATH);

int16_t getMeasurementSaveRecordSize()
{
  return sizeof(StoredMeasuredResult);
}

int16_t getSeriesSaveRecordSize()
{
  return sizeof(StoredMeasuredSeries);
}

// One-time migration of a records file from a legacy (pre-'name') record layout
// to the current one. Valid records get their 'name' field pre-filled with the
// formatted record number (the way records were displayed before the name field
// existed), deleted/empty slots keep an empty name. The whole file is rewritten
// with the new fixed record size.
template <typename OldRecordT, typename NewRecordT>
void migrateRecordsFileFromLegacy(const char *filePath, int32_t totalCount)
{
  int32_t newBlockSize = sizeof(NewRecordT);
  int32_t oldBlockSize = sizeof(OldRecordT);

  FILE *recordsFile = fopen(filePath, "r");

  if (recordsFile == NULL)
  {
    return; // migrировать нечего, initRecordsFile() создаст файл
  }

  ESP_LOGW("", "Migrating '%s' from record size %" PRIi32 " to %" PRIi32 "...", filePath, oldBlockSize, newBlockSize);

  std::vector<NewRecordT> records;
  records.reserve(totalCount);

  OldRecordT old;
  while (fread(&old, oldBlockSize, 1, recordsFile) == 1)
  {
    NewRecordT rec;
    memset(&rec, 0, sizeof(rec));

    // Общая область полей (все поля до 'name' совпадают в обоих форматах)
    memcpy(&rec, &old, offsetof(NewRecordT, name));

    // Легаси-записи не имели имени - заполняем его форматированным номером
    if (old.recordNumber > 0 && !old.isDeleted)
    {
      formatRecordName(old.recordNumber, rec.name);
    }

    records.push_back(rec);
  }

  fclose(recordsFile);

  recordsFile = fopen(filePath, "w");

  if (recordsFile == NULL)
  {
    halt("Halted. Failed to open records file for migration");
  }

  for (size_t i = 0; i < records.size(); i++)
  {
    if (fwrite(&records[i], newBlockSize, 1, recordsFile) != 1)
    {
      halt("Halted. Failed to write migrated record");
    }
  }

  // Дописываем пустые слоты до фиксированного общего количества записей
  NewRecordT empty;
  memset(&empty, 0, sizeof(empty));

  for (int32_t i = (int32_t)records.size(); i < totalCount; i++)
  {
    if (fwrite(&empty, newBlockSize, 1, recordsFile) != 1)
    {
      halt("Halted. Failed to write initial data to records file");
    }
  }

  fclose(recordsFile);
  ESP_LOGI("", "'%s' migrated (%u records)", filePath, (unsigned)records.size());
}

// Проверяет, что файл записей существует и использует текущий формат записи.
// Легаси-файлы (размер под старую структуру) мигрируются, файлы с неожидаемым
// размером пересоздаются с нуля.
template <typename OldRecordT, typename NewRecordT>
void ensureRecordsFileUpToDate(const char *filePath, int32_t totalCount)
{
  struct stat st;

  if (stat(filePath, &st) != 0)
  {
    return; // файла нет - initRecordsFile() создаст его
  }

  int32_t expectedSize = totalCount * (int32_t)sizeof(NewRecordT);

  if (st.st_size == expectedSize)
  {
    return; // файл уже в актуальном формате
  }

  int32_t legacySize = totalCount * (int32_t)sizeof(OldRecordT);

  if (st.st_size == legacySize)
  {
    migrateRecordsFileFromLegacy<OldRecordT, NewRecordT>(filePath, totalCount);
    return;
  }

  ESP_LOGE("", "'%s' has unexpected size %" PRIi32 " (expected %" PRIi32 " or legacy %" PRIi32 "). Recreating.", filePath, (int32_t)st.st_size, expectedSize, legacySize);
  remove(filePath);
}

// Creates and zero-fills a fixed-size binary records file if it does not exist,
// then reads it back to verify the written zeroes.
void initRecordsFile(const char *filePath, int32_t totalCount, int16_t blockSize)
{
  struct stat st;

  if (stat(filePath, &st) == 0)
  {
    return; // file already exists 
  }

  ESP_LOGI("", "No '%s' file found. Initializing...", filePath);

  uint8_t data[blockSize] = {};
  FILE *recordsFile = fopen(filePath, "w");

  for (int32_t i = 0; i < totalCount; i++)
  {
    size_t res = fwrite(data, blockSize, 1, recordsFile);

    if (res == 0)
    {
      halt("Halted. Failed to write initial data to records file");
    }
  }

  ESP_LOGI("", "'%s' initialized", filePath);
  fclose(recordsFile);

  ESP_LOGI("", "Verifing writen data...");

  recordsFile = fopen(filePath, "r");

  uint8_t buf[1] = {};

  for (int32_t i = 0; i < totalCount * blockSize; i++)
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

  // Миграция легаси-файлов записей (если формат записи изменился)
  ensureRecordsFileUpToDate<OldStoredMeasuredResult, StoredMeasuredResult>(RECORDS_FILE_PATH, TOTAL_RECORDS_COUNT);
  ensureRecordsFileUpToDate<OldStoredMeasuredSeries, StoredMeasuredSeries>(SERIES_FILE_PATH, TOTAL_SERIES_COUNT);

  initRecordsFile(RECORDS_FILE_PATH, TOTAL_RECORDS_COUNT, getMeasurementSaveRecordSize());
  initRecordsFile(SERIES_FILE_PATH, TOTAL_SERIES_COUNT, getSeriesSaveRecordSize());
}
