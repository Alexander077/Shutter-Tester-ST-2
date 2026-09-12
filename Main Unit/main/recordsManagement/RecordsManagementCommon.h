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

// Creates and zero-fills a fixed-size binary records file if it does not exist,
// then reads it back to verify the written zeroes.
void initRecordsFile(const char *filePath, int32_t totalCount, int16_t blockSize);

// --- Migration from the nameless record layout (ai task 6) ---
// Old on-flash layouts: identical to the current structs but without the
// trailing 'name' field. Kept only to recognize and convert files written
// by previous firmware versions.
struct OldStoredMeasuredResult
{
  int32_t recordNumber;
  bool isDeleted;

  double sensor0Time;
  double sensor1Time;

  double curtain1spanAtime;
  double curtain1spanAspeed;
  double curtain1TotalTime;

  double curtain2spanAtime;
  double curtain2spanAspeed;
  double curtain2TotalTime;

  double slitWidthSensor0;
  double slitWidthSensor1;
  double slitWidthAverage;

  SensorType usedSensorType;
  CurtainMovement selectedCurtainMovement;
};

struct OldStoredMeasuredSeries
{
  int32_t recordNumber;
  bool isDeleted;
  uint8_t measurementCount;
  MeasuredResult results[MEASUREMENT_SERIES_MAX_SIZE];
};

// Forward declaration: full definition sits below (after the byte-size
// getters) so this header keeps its original layout; the migration template
// only calls it at instantiation time from initStorage().
void initRecordsFile(const char *filePath, int32_t totalCount, int16_t blockSize);

// Reads a same-slot old-format file and rewrites it in the new format,
// filling 'name' with the legacy "0002"-style number. Records keep their
// slot positions and numbers (direct positional write, not via the manager,
// so numbers are preserved exactly).
template <typename NewT, typename OldT>
void migrateRecordsFileToNamed(const char *filePath, int32_t totalCount)
{
  struct stat st;

  if (stat(filePath, &st) != 0)
  {
    return;
  }

  const long newExpected = (long)totalCount * (long)sizeof(NewT);
  const long oldExpected = (long)totalCount * (long)sizeof(OldT);

  if ((long)st.st_size == newExpected)
  {
    return; // already in the new format
  }

  if ((long)st.st_size != oldExpected)
  {
    ESP_LOGE("", "Records file '%s' has unexpected size %ld (new=%ld old=%ld). Recreating.",
             filePath, (long)st.st_size, newExpected, oldExpected);
    remove(filePath);
    initRecordsFile(filePath, totalCount, sizeof(NewT));
    return;
  }

  ESP_LOGI("", "Migrating '%s' to named records (%ld -> %ld bytes)", filePath, (long)st.st_size, newExpected);

  FILE *oldFile = fopen(filePath, "r");

  if (oldFile == NULL)
  {
    ESP_LOGE("", "Failed to open '%s' for migration", filePath);
    return;
  }

  // 100 slots max: old series ~600 bytes each -> ~60 KB static buffer.
  static OldT oldBuf[TOTAL_RECORDS_COUNT > TOTAL_SERIES_COUNT ? TOTAL_RECORDS_COUNT : TOTAL_SERIES_COUNT];

  size_t readCount = fread(oldBuf, sizeof(OldT), totalCount, oldFile);
  fclose(oldFile);

  if ((int32_t)readCount != totalCount)
  {
    ESP_LOGE("", "Failed to read '%s' for migration", filePath);
    remove(filePath);
    initRecordsFile(filePath, totalCount, sizeof(NewT));
    return;
  }

  remove(filePath);
  initRecordsFile(filePath, totalCount, sizeof(NewT));

  FILE *newFile = fopen(filePath, "r+");

  if (newFile == NULL)
  {
    ESP_LOGE("", "Failed to recreate '%s' for migration", filePath);
    return;
  }

  for (int32_t i = 0; i < totalCount; i++)
  {
    NewT fresh;
    memset(&fresh, 0, sizeof(NewT));
    memcpy(&fresh, &oldBuf[i], sizeof(OldT));

    if (oldBuf[i].recordNumber > 0)
    {
      formatDefaultRecordName(oldBuf[i].recordNumber, fresh.name);
    }

    fseek(newFile, i * (long)sizeof(NewT), SEEK_SET);
    fwrite(&fresh, sizeof(NewT), 1, newFile);
  }

  fclose(newFile);
  ESP_LOGI("", "Migration of '%s' done", filePath);
}

// Fills empty names of an already-new-format file with default number names.
template <typename ManagerT, typename RecordT>
void fillEmptyRecordNames(ManagerT &manager)
{
  std::vector<int32_t> ids = manager.getAllValidRecordNumbers();

  for (int32_t id : ids)
  {
    RecordT rec = manager.getRecordByNumber(id);

    if (rec.recordNumber <= 0 || rec.isDeleted)
    {
      continue;
    }

    if (rec.name[0] == '\0' || isRecordNameEmpty(rec.name))
    {
      formatDefaultRecordName(rec.recordNumber, rec.name);
      manager.saveOrUpdateRecord(rec);
    }
    else
    {
      // Guarantee termination even if an old bug wrote a non-terminated name.
      rec.name[RECORD_NAME_MAX_LEN] = '\0';
    }
  }
}

// Case-insensitive duplicate check inside one storage (Q1: separately;
// Q2: the record being overwritten is excluded via excludeRecordNumber).
// Stored records with empty names never count as duplicates.
template <typename ManagerT, typename RecordT>
bool isRecordNameTaken(ManagerT &manager, const char *name, int32_t excludeRecordNumber = -1)
{
  if (isRecordNameEmpty(name))
  {
    return false;
  }

  std::vector<int32_t> ids = manager.getAllValidRecordNumbers();

  for (int32_t id : ids)
  {
    if (id == excludeRecordNumber)
    {
      continue;
    }

    RecordT rec = manager.getRecordByNumber(id);

    if (rec.recordNumber <= 0 || rec.isDeleted || isRecordNameEmpty(rec.name))
    {
      continue;
    }

    if (recordNamesEqualIgnoreCase(rec.name, name))
    {
      return true;
    }
  }

  return false;
}

bool isMeasurementRecordNameTaken(const char *name, int32_t excludeRecordNumber = -1)
{
  return isRecordNameTaken<RecordsStorageManager, StoredMeasuredResult>(storage, name, excludeRecordNumber);
}

bool isSeriesRecordNameTaken(const char *name, int32_t excludeRecordNumber = -1)
{
  return isRecordNameTaken<SeriesStorageManager, StoredMeasuredSeries>(seriesStorage, name, excludeRecordNumber);
}

// Next free record number (max valid id + 1) for the "Save as 'xxxx'" label.
template <typename ManagerT>
int32_t getNextRecordNumber(ManagerT &manager)
{
  std::vector<int32_t> ids = manager.getAllValidRecordNumbers();
  int32_t maxId = 0;

  for (int32_t id : ids)
  {
    if (id > maxId)
    {
      maxId = id;
    }
  }

  return maxId + 1;
}

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
void initRecordsFileImpl(const char *filePath, int32_t totalCount, int16_t blockSize)
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

void initRecordsFile(const char *filePath, int32_t totalCount, int16_t blockSize)
{
  initRecordsFileImpl(filePath, totalCount, blockSize);
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

  // Migrate old nameless files (same-slot positional rewrite, numbers kept).
  migrateRecordsFileToNamed<StoredMeasuredResult, OldStoredMeasuredResult>(RECORDS_FILE_PATH, TOTAL_RECORDS_COUNT);
  migrateRecordsFileToNamed<StoredMeasuredSeries, OldStoredMeasuredSeries>(SERIES_FILE_PATH, TOTAL_SERIES_COUNT);
}
