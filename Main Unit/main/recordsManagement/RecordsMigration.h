#pragma once

// Migration of legacy record files (without the 'name' field) to the current
// format. Detects the old layout by file size, converts every slot in RAM and
// rewrites the whole file atomically (temp file + rename, backup kept).

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "lib/Common.h"
#include "lib/MeasuredResult.h"
#include "recordsManagement/StoredMeasuredResult.h"
#include "recordsManagement/StoredMeasuredSeries.h"

// Old layouts (the 'name' field did not exist yet).
struct LegacyStoredMeasuredResult
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

struct LegacyStoredMeasuredSeries
{
  int32_t recordNumber;
  bool isDeleted;
  uint8_t measurementCount;
  MeasuredResult results[MEASUREMENT_SERIES_MAX_SIZE];
};

// Copies a whole file byte-by-byte. Used for the pre-migration backup.
static bool migrateCopyFile(const char *srcPath, const char *dstPath)
{
  FILE *src = fopen(srcPath, "r");
  if (src == NULL)
  {
    return false;
  }

  FILE *dst = fopen(dstPath, "w");
  if (dst == NULL)
  {
    fclose(src);
    return false;
  }

  uint8_t buf[256];
  size_t n;
  bool ok = true;

  while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
  {
    if (fwrite(buf, 1, n, dst) != n)
    {
      ok = false;
      break;
    }
  }

  fclose(src);
  fclose(dst);
  return ok;
}

// Writes the converted buffer via temp file + rename. Shared by both files.
// (Kept for compatibility; the per-record streaming functions above no longer
// use it, so it stays unused until the next migration version needs it.)
static bool __attribute__((unused)) migrateWriteThroughTemp(const char *filePath, const void *data, size_t dataSize)
{
  char backupPath[64];
  char tmpPath[64];
  snprintf(backupPath, sizeof(backupPath), "%s.bak", filePath);
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", filePath);

  if (!migrateCopyFile(filePath, backupPath))
  {
    ESP_LOGE("", "Records migration: cannot create backup of '%s'", filePath);
    return false;
  }

  FILE *tmpFile = fopen(tmpPath, "w");
  if (tmpFile == NULL)
  {
    ESP_LOGE("", "Records migration: cannot create temp file '%s'", tmpPath);
    return false;
  }

  bool writeOk = (fwrite(data, dataSize, 1, tmpFile) == 1);
  fclose(tmpFile);

  if (!writeOk)
  {
    ESP_LOGE("", "Records migration: failed to write temp file '%s'", tmpPath);
    remove(tmpPath);
    return false;
  }

  if (rename(tmpPath, filePath) != 0)
  {
    ESP_LOGE("", "Records migration: failed to replace '%s'", filePath);
    remove(tmpPath);
    return false;
  }

  ESP_LOGI("", "Records migration: '%s' converted OK (backup: '%s')", filePath, backupPath);
  return true;
}

// Migrates single-measurement records. Returns true when no migration was
// needed or it succeeded, false on I/O error (retried on the next boot).
// Zero heap/stack pressure: converts record-by-record from the backup file
// into the temp file (only two small structs on the stack), so the next
// fopen never trips the TLSF allocator.
static bool migrateSingleRecordsFile(const char *filePath, int32_t totalCount)
{
  struct stat st;
  if (stat(filePath, &st) != 0)
  {
    return true; // file does not exist yet, initRecordsFile() will create it
  }

  const long newSize = (long)totalCount * (long)sizeof(StoredMeasuredResult);
  if (st.st_size == newSize)
  {
    return true; // already migrated
  }

  const long oldSize = (long)totalCount * (long)sizeof(LegacyStoredMeasuredResult);
  if (st.st_size != oldSize)
  {
    ESP_LOGE("", "Records migration: unexpected size %ld for '%s'", (long)st.st_size, filePath);
    return false;
  }

  ESP_LOGI("", "Records migration: converting '%s' to named format...", filePath);

  char backupPath[64];
  char tmpPath[64];
  snprintf(backupPath, sizeof(backupPath), "%s.bak", filePath);
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", filePath);

  if (!migrateCopyFile(filePath, backupPath))
  {
    ESP_LOGE("", "Records migration: cannot create backup of '%s'", filePath);
    return false;
  }

  // Convert from the backup (not from the live file): if power fails mid-way,
  // the original file is still intact and migration retries on next boot.
  FILE *oldFile = fopen(backupPath, "r");
  if (oldFile == NULL)
  {
    ESP_LOGE("", "Records migration: cannot open backup '%s' for reading", backupPath);
    return false;
  }

  FILE *tmpFile = fopen(tmpPath, "w");
  if (tmpFile == NULL)
  {
    ESP_LOGE("", "Records migration: cannot create temp file '%s'", tmpPath);
    fclose(oldFile);
    return false;
  }

  bool ok = true;

  for (int32_t i = 0; i < totalCount; i++)
  {
    LegacyStoredMeasuredResult legacy;
    memset(&legacy, 0, sizeof(legacy));

    if (fread(&legacy, sizeof(legacy), 1, oldFile) != 1)
    {
      ESP_LOGE("", "Records migration: failed to read '%s'", filePath);
      ok = false;
      break;
    }

    StoredMeasuredResult dst;
    memset(&dst, 0, sizeof(dst));
    dst.recordNumber = legacy.recordNumber;
    dst.isDeleted = legacy.isDeleted;
    dst.sensor0Time = legacy.sensor0Time;
    dst.sensor1Time = legacy.sensor1Time;
    dst.curtain1spanAtime = legacy.curtain1spanAtime;
    dst.curtain1spanAspeed = legacy.curtain1spanAspeed;
    dst.curtain1TotalTime = legacy.curtain1TotalTime;
    dst.curtain2spanAtime = legacy.curtain2spanAtime;
    dst.curtain2spanAspeed = legacy.curtain2spanAspeed;
    dst.curtain2TotalTime = legacy.curtain2TotalTime;
    dst.slitWidthSensor0 = legacy.slitWidthSensor0;
    dst.slitWidthSensor1 = legacy.slitWidthSensor1;
    dst.slitWidthAverage = legacy.slitWidthAverage;
    dst.usedSensorType = legacy.usedSensorType;
    dst.selectedCurtainMovement = legacy.selectedCurtainMovement;

    if (legacy.recordNumber > 0 && !legacy.isDeleted)
    {
      formatRecordName(legacy.recordNumber, dst.name);
    }

    if (fwrite(&dst, sizeof(dst), 1, tmpFile) != 1)
    {
      ESP_LOGE("", "Records migration: failed to write temp file '%s'", tmpPath);
      ok = false;
      break;
    }
  }

  fclose(oldFile);
  fclose(tmpFile);

  if (!ok)
  {
    remove(tmpPath);
    return false;
  }

  if (rename(tmpPath, filePath) != 0)
  {
    ESP_LOGE("", "Records migration: failed to replace '%s'", filePath);
    remove(tmpPath);
    return false;
  }

  ESP_LOGI("", "Records migration: '%s' converted OK (backup: '%s')", filePath, backupPath);
  return true;
}

// Same for the series file.
static bool migrateSeriesRecordsFile(const char *filePath, int32_t totalCount)
{
  struct stat st;
  if (stat(filePath, &st) != 0)
  {
    return true;
  }

  const long newSize = (long)totalCount * (long)sizeof(StoredMeasuredSeries);
  if (st.st_size == newSize)
  {
    return true;
  }

  const long oldSize = (long)totalCount * (long)sizeof(LegacyStoredMeasuredSeries);
  if (st.st_size != oldSize)
  {
    ESP_LOGE("", "Records migration: unexpected size %ld for '%s'", (long)st.st_size, filePath);
    return false;
  }

  ESP_LOGI("", "Records migration: converting '%s' to named format...", filePath);

  char backupPath[64];
  char tmpPath[64];
  snprintf(backupPath, sizeof(backupPath), "%s.bak", filePath);
  snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", filePath);

  if (!migrateCopyFile(filePath, backupPath))
  {
    ESP_LOGE("", "Records migration: cannot create backup of '%s'", filePath);
    return false;
  }

  // Convert from the backup: power failure mid-way leaves the live file
  // intact and migration retries on next boot.
  FILE *oldFile = fopen(backupPath, "r");
  if (oldFile == NULL)
  {
    ESP_LOGE("", "Records migration: cannot open backup '%s' for reading", backupPath);
    return false;
  }

  FILE *tmpFile = fopen(tmpPath, "w");
  if (tmpFile == NULL)
  {
    ESP_LOGE("", "Records migration: cannot create temp file '%s'", tmpPath);
    fclose(oldFile);
    return false;
  }

  bool ok = true;

  for (int32_t i = 0; i < totalCount; i++)
  {
    LegacyStoredMeasuredSeries legacy;
    memset(&legacy, 0, sizeof(legacy));

    if (fread(&legacy, sizeof(legacy), 1, oldFile) != 1)
    {
      ESP_LOGE("", "Records migration: failed to read '%s'", filePath);
      ok = false;
      break;
    }

    StoredMeasuredSeries dst;
    memset(&dst, 0, sizeof(dst));
    dst.recordNumber = legacy.recordNumber;
    dst.isDeleted = legacy.isDeleted;
    dst.measurementCount = legacy.measurementCount;
    for (uint8_t m = 0; m < MEASUREMENT_SERIES_MAX_SIZE; m++)
    {
      dst.results[m] = legacy.results[m];
    }

    if (legacy.recordNumber > 0 && !legacy.isDeleted)
    {
      formatRecordName(legacy.recordNumber, dst.name);
    }

    if (fwrite(&dst, sizeof(dst), 1, tmpFile) != 1)
    {
      ESP_LOGE("", "Records migration: failed to write temp file '%s'", tmpPath);
      ok = false;
      break;
    }
  }

  fclose(oldFile);
  fclose(tmpFile);

  if (!ok)
  {
    remove(tmpPath);
    return false;
  }

  if (rename(tmpPath, filePath) != 0)
  {
    ESP_LOGE("", "Records migration: failed to replace '%s'", filePath);
    remove(tmpPath);
    return false;
  }

  ESP_LOGI("", "Records migration: '%s' converted OK (backup: '%s')", filePath, backupPath);
  return true;
}

// Entry point: migrate both files if they still have the legacy layout.
// Must be called after initStorage() (files exist) and before storage.begin().
static void migrateLegacyRecordsIfNeeded()
{
  bool okSingle = migrateSingleRecordsFile(RECORDS_FILE_PATH, TOTAL_RECORDS_COUNT);
  bool okSeries = migrateSeriesRecordsFile(SERIES_FILE_PATH, TOTAL_SERIES_COUNT);

  if (!okSingle || !okSeries)
  {
    ESP_LOGE("", "Records migration finished with errors, will retry on next boot");
  }
}
