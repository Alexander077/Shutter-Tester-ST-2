#pragma once

#include <Arduino.h>
#include <vector>
#include <string.h>
#include "StoredMeasuredResult.h"
#include "freertos/semphr.h"
#include "esp_log.h"

class RecordsStorageManager
{
private:
  const char *filePath;
  SemaphoreHandle_t fileMutex;

public:
  // Конструктор
  RecordsStorageManager(const char *path);

  // Инициализация (создание мьютекса)
  void begin();

  // Получить список всех ID существующих записей
  std::vector<int32_t> getAllValidRecordNumbers();

  // Получить конкретную запись по её номеру
  StoredMeasuredResult getRecordByNumber(int32_t recordNumber);

  // Удалить запись по номеру
  bool deleteRecordByNumber(int32_t recordNumber);

  // Сохранить новую или обновить существующую запись
  // Возвращает номер сохраненной записи (или -1 в случае ошибки)
  int32_t saveOrUpdateRecord(StoredMeasuredResult &record);

  // Получить количество свободных слотов (если файл имеет фиксированный размер/структуру)
  int16_t getFreeSlotsCount();
};

RecordsStorageManager::RecordsStorageManager(const char *path)
{
  filePath = path;
  fileMutex = NULL;
}

void RecordsStorageManager::begin()
{
  fileMutex = xSemaphoreCreateMutex();
  
  if (fileMutex == NULL)
  {
    ESP_LOGE("STORAGE", "Failed to create file mutex");
  }
}

std::vector<int32_t> RecordsStorageManager::getAllValidRecordNumbers()
{
  std::vector<int32_t> records;

  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");

    if (recordsFile != NULL)
    {
      StoredMeasuredResult tempMeasRes;
      int16_t blockSize = sizeof(StoredMeasuredResult);

      while (fread(&tempMeasRes, blockSize, 1, recordsFile) == 1)
      {
        if (tempMeasRes.recordNumber > 0 && !tempMeasRes.isDeleted)
        {
          records.push_back(tempMeasRes.recordNumber);
        }
      }

      fclose(recordsFile);
    }
    else
    {
      ESP_LOGE("STORAGE", "Failed to open records file for reading list");
    }

    xSemaphoreGive(fileMutex);
  }
  return records;
}

StoredMeasuredResult RecordsStorageManager::getRecordByNumber(int32_t recordNumber)
{
  StoredMeasuredResult measRes;
  memset(&measRes, 0, sizeof(StoredMeasuredResult));
  measRes.recordNumber = -1; // Признак того, что запись не найдена

  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");
    if (recordsFile != NULL)
    {
      StoredMeasuredResult tempMeasRes;
      int16_t blockSize = sizeof(StoredMeasuredResult);
      while (fread(&tempMeasRes, blockSize, 1, recordsFile) == 1)
      {
        if (tempMeasRes.recordNumber == recordNumber && !tempMeasRes.isDeleted)
        {
          measRes = tempMeasRes;
          break;
        }
      }
      fclose(recordsFile);
    }
    xSemaphoreGive(fileMutex);
  }
  return measRes;
}

bool RecordsStorageManager::deleteRecordByNumber(int32_t recordNumber)
{
  bool success = false;
  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r+");
    if (recordsFile != NULL)
    {
      StoredMeasuredResult tempMeasRes;
      int16_t blockSize = sizeof(StoredMeasuredResult);
      int32_t index = 0;

      while (fread(&tempMeasRes, blockSize, 1, recordsFile) == 1)
      {
        if (tempMeasRes.recordNumber == recordNumber && !tempMeasRes.isDeleted)
        {
          tempMeasRes.isDeleted = true;
          fseek(recordsFile, index * blockSize, SEEK_SET);
          if (fwrite(&tempMeasRes, blockSize, 1, recordsFile) == 1)
          {
            success = true;
          }
          break;
        }
        index++;
      }
      fclose(recordsFile);
    }
    xSemaphoreGive(fileMutex);
  }
  return success;
}

int32_t RecordsStorageManager::saveOrUpdateRecord(StoredMeasuredResult &record)
{
  int32_t finalRecordNumber = -1;

  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r+");
    // Если файла нет, пытаемся создать его
    if (recordsFile == NULL)
    {
      recordsFile = fopen(filePath, "w+");
    }

    if (recordsFile != NULL)
    {
      int16_t blockSize = sizeof(StoredMeasuredResult);
      StoredMeasuredResult tempMeasRes;
      int32_t index = 0;
      int32_t maxRecordNumber = 0;
      int32_t targetIndex = -1;
      int32_t freeIndex = -1;

      // Поиск индексов и максимального номера
      while (fread(&tempMeasRes, blockSize, 1, recordsFile) == 1)
      {
        if (tempMeasRes.recordNumber > maxRecordNumber && !tempMeasRes.isDeleted)
        {
          maxRecordNumber = tempMeasRes.recordNumber;
        }
        if (record.recordNumber > 0 && tempMeasRes.recordNumber == record.recordNumber && !tempMeasRes.isDeleted)
        {
          targetIndex = index;
        }
        if ((tempMeasRes.recordNumber == 0 || tempMeasRes.isDeleted) && freeIndex == -1)
        {
          freeIndex = index;
        }

        index++;
      }

      if (record.recordNumber > 0 && targetIndex != -1)
      {
        // Update (обновление существующей)
        fseek(recordsFile, targetIndex * blockSize, SEEK_SET);

        if (fwrite(&record, blockSize, 1, recordsFile) == 1)
        {
          finalRecordNumber = record.recordNumber;
        }
      }
      else
      {
        // Create (создание новой)
        finalRecordNumber = maxRecordNumber + 1;
        record.recordNumber = finalRecordNumber;

        // Если есть свободный слот (удаленный или пустой), пишем туда, иначе в конец
        if (freeIndex != -1)
        {
          fseek(recordsFile, freeIndex * blockSize, SEEK_SET);
        }
        else
        {
          fseek(recordsFile, 0, SEEK_END);
        }

        if (fwrite(&record, blockSize, 1, recordsFile) != 1)
        {
          finalRecordNumber = -1; // Ошибка записи
        }
      }

      fclose(recordsFile);
    }
    else
    {
      ESP_LOGE("STORAGE", "Failed to open or create records file for writing");
    }
    xSemaphoreGive(fileMutex);
  }
  return finalRecordNumber;
}

int16_t RecordsStorageManager::getFreeSlotsCount()
{
  int16_t freeCount = 0;
  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");
    if (recordsFile != NULL)
    {
      StoredMeasuredResult tempMeasRes;
      int16_t blockSize = sizeof(StoredMeasuredResult);
      while (fread(&tempMeasRes, blockSize, 1, recordsFile) == 1)
      {
        if (tempMeasRes.recordNumber == 0 || tempMeasRes.isDeleted)
        {
          freeCount++;
        }
      }
      fclose(recordsFile);
    }
    xSemaphoreGive(fileMutex);
  }
  return freeCount;
}