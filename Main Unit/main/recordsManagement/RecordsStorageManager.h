#pragma once

#include <Arduino.h>
#include <vector>
#include <string.h>
#include "recordsManagement/StoredMeasuredResult.h"
#include "recordsManagement/StoredMeasuredSeries.h"
#include "freertos/semphr.h"
#include "esp_log.h"

// Generic manager over a fixed-size binary file of equally sized records.
// It only relies on the first two fields of the record type being
// 'recordNumber' (int32_t) and 'isDeleted' (bool), so the same implementation
// serves both single measurements and measurement series.
template <typename RecordT>
class RecordsStorageManagerT
{
private:
  const char *filePath;
  SemaphoreHandle_t fileMutex;

public:
  // Конструктор
  RecordsStorageManagerT(const char *path);

  // Инициализация (создание мьютекса)
  void begin();

  // Получить список всех ID существующих записей
  std::vector<int32_t> getAllValidRecordNumbers();

  // Получить список всех существующих записей (номер + имя).
  // Используется экранами выбора записей и проверкой дублей имен.
  std::vector<RecordT> getAllValidRecords();

  // Проверить, занято ли имя другой записью (точное совпадение, регистр важен).
  // excludeRecordNumber исключается из проверки (нужен при переименовании).
  bool isNameTaken(const char *name, int32_t excludeRecordNumber = -1);

  // Получить конкретную запись по её номеру
  RecordT getRecordByNumber(int32_t recordNumber);

  // Удалить запись по номеру
  bool deleteRecordByNumber(int32_t recordNumber);

  // Сохранить новую или обновить существующую запись
  // Возвращает номер сохраненной записи (или -1 в случае ошибки)
  int32_t saveOrUpdateRecord(RecordT &record);

  // Получить количество свободных слотов (если файл имеет фиксированный размер/структуру)
  int16_t getFreeSlotsCount();
};

// Concrete managers used across the project.
using RecordsStorageManager = RecordsStorageManagerT<StoredMeasuredResult>;
using SeriesStorageManager = RecordsStorageManagerT<StoredMeasuredSeries>;

template <typename RecordT>
RecordsStorageManagerT<RecordT>::RecordsStorageManagerT(const char *path)
{
  filePath = path;
  fileMutex = NULL;
}

template <typename RecordT>
void RecordsStorageManagerT<RecordT>::begin()
{
  fileMutex = xSemaphoreCreateMutex();

  if (fileMutex == NULL)
  {
    ESP_LOGE("STORAGE", "Failed to create file mutex");
  }
}

template <typename RecordT>
std::vector<int32_t> RecordsStorageManagerT<RecordT>::getAllValidRecordNumbers()
{
  std::vector<int32_t> records;

  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");

    if (recordsFile != NULL)
    {
      RecordT tempMeasRes;
      int16_t blockSize = sizeof(RecordT);

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

template <typename RecordT>
std::vector<RecordT> RecordsStorageManagerT<RecordT>::getAllValidRecords()
{
  std::vector<RecordT> records;

  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");

    if (recordsFile != NULL)
    {
      RecordT tempMeasRes;
      int16_t blockSize = sizeof(RecordT);

      while (fread(&tempMeasRes, blockSize, 1, recordsFile) == 1)
      {
        if (tempMeasRes.recordNumber > 0 && !tempMeasRes.isDeleted)
        {
          records.push_back(tempMeasRes);
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

template <typename RecordT>
bool RecordsStorageManagerT<RecordT>::isNameTaken(const char *name, int32_t excludeRecordNumber)
{
  if (name == NULL || name[0] == '\0')
  {
    return false;
  }

  std::vector<RecordT> records = getAllValidRecords();

  for (size_t i = 0; i < records.size(); i++)
  {
    if (records[i].recordNumber == excludeRecordNumber)
    {
      continue;
    }

    if (strncmp(records[i].name, name, RECORD_NAME_BUF_SIZE) == 0)
    {
      return true;
    }
  }

  return false;
}

template <typename RecordT>
RecordT RecordsStorageManagerT<RecordT>::getRecordByNumber(int32_t recordNumber)
{
  RecordT measRes;
  memset(&measRes, 0, sizeof(RecordT));
  measRes.recordNumber = -1; // Признак того, что запись не найдена

  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");
    if (recordsFile != NULL)
    {
      RecordT tempMeasRes;
      int16_t blockSize = sizeof(RecordT);
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

template <typename RecordT>
bool RecordsStorageManagerT<RecordT>::deleteRecordByNumber(int32_t recordNumber)
{
  bool success = false;
  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r+");
    if (recordsFile != NULL)
    {
      RecordT tempMeasRes;
      int16_t blockSize = sizeof(RecordT);
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

template <typename RecordT>
int32_t RecordsStorageManagerT<RecordT>::saveOrUpdateRecord(RecordT &record)
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
      int16_t blockSize = sizeof(RecordT);
      RecordT tempMeasRes;
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

template <typename RecordT>
int16_t RecordsStorageManagerT<RecordT>::getFreeSlotsCount()
{
  int16_t freeCount = 0;
  if (fileMutex != NULL && xSemaphoreTake(fileMutex, portMAX_DELAY))
  {
    FILE *recordsFile = fopen(filePath, "r");
    if (recordsFile != NULL)
    {
      RecordT tempMeasRes;
      int16_t blockSize = sizeof(RecordT);
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

