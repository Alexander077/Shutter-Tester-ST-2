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
#include "recordsManagement/StoredMeasuredSeries.h"
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

  initRecordsFile(RECORDS_FILE_PATH, TOTAL_RECORDS_COUNT, getMeasurementSaveRecordSize());
  initRecordsFile(SERIES_FILE_PATH, TOTAL_SERIES_COUNT, getSeriesSaveRecordSize());
}
