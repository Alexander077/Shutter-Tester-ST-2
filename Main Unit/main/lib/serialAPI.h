#pragma once

#include <Arduino.h>
#include <unistd.h>
#include <fcntl.h>
#include "cJSON.h"
#include "Common.h"
#include "StoredMeasuredResult.h"
#include "RecordsStorageManager.h"
#include "lib/FirmwareUpdate.h"

// Глобальный флаг для запроса API
volatile bool isApiRequestReceived = false;
// Объявляем глобальный экземпляр хранилища
extern RecordsStorageManager storage;

enum class ApiRequstAction
{
  NO_ACTION,
  GO_TO_LIGHT_SETUP,
  GO_TO_MEASURE
};

volatile ApiRequstAction apiRequestAction = ApiRequstAction::NO_ACTION;

class SerialAPILightStatus
{
public:
  static constexpr const char *LIGHT_STATUS_TOO_DIMM = "LIGHT_STATUS_TOO_DIM";
  static constexpr const char *LIGHT_STATUS_TOO_BRIGHT = "LIGHT_STATUS_TOO_BRIGHT";
  static constexpr const char *LIGHT_STATUS_OK = "LIGHT_STATUS_OK";
};

class SerialAPIRequestAction
{
public:
  static constexpr const char *API_REQUEST_LIGHT_SETUP = "API_REQUEST_LIGHT_SETUP";
  static constexpr const char *API_REQUEST_MEASURE = "API_REQUEST_MEASURE";
  static constexpr const char *API_REQUEST_GET_RECORDS_LIST = "API_REQUEST_GET_RECORDS_LIST";
  static constexpr const char *API_REQUEST_GET_RECORD = "API_REQUEST_GET_RECORD";
  static constexpr const char *API_REQUEST_DELETE_RECORD = "API_REQUEST_DELETE_RECORD";
  static constexpr const char *API_REQUEST_SAVE_RECORD = "API_REQUEST_SAVE_RECORD";
  static constexpr const char *API_REQUEST_FIRMWARE_UPDATE = "API_REQUEST_FIRMWARE_UPDATE";
};

class SerialAPIResponse
{
public:
  static constexpr const char *API_RESPONSE_STATUS_OK = "API_RESPONSE_STATUS_OK";
  static constexpr const char *API_RESPONSE_STATUS_ERROR = "API_RESPONSE_STATUS_ERROR";
};

char *serialApiLightQualityStatusesStr[3] = {"LIGHT_QUALITY_UNKNOWN", "LIGHT_QUALITY_OK", "LIGHT_QUALITY_BAD"};

void serialApiTask(void *pvParameters)
{
  printf("[API_FLOW] --- Task serialApiTask started ---\n");
  fflush(stdout);

  const size_t API_BUF_SIZE = 1024;
  char rxBuf[API_BUF_SIZE];
  size_t rxIdx = 0; // Индекс для накопления символов в буфере

  setvbuf(stdin, NULL, _IONBF, 0);
  printf("[API_FLOW] stdin set to _IONBF (unbuffered) mode\n");
  fflush(stdout);

  while (true)
  {
    int c = fgetc(stdin); // Читаем по одному символу из USB CDC

    if (c == EOF)
    {
      // Данных пока нет, ждем немного, чтобы не грузить процессор
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    // Если пришел символ конца строки - значит команда (весь JSON) получена
    if (c == '\n' || c == '\r')
    {
      if (rxIdx > 0)
      {
        rxBuf[rxIdx] = '\0'; // Закрываем строку

        printf("[API_FLOW] >>> String assembled (Len: %d): %s\n", rxIdx, rxBuf);
        fflush(stdout);

        cJSON *json = cJSON_Parse(rxBuf);

        if (json != NULL)
        {
          printf("[API_FLOW] JSON parsed successfully\n");
          fflush(stdout);

          cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(json, "cmd");

          if (cJSON_IsString(cmd_item) && (cmd_item->valuestring != NULL))
          {
            printf("[API_FLOW] Command extracted: [%s]\n", cmd_item->valuestring);
            fflush(stdout);

            if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_LIGHT_SETUP) == 0)
            {
              printf("[API_FLOW] -> Branch: %s\n", SerialAPIRequestAction::API_REQUEST_LIGHT_SETUP);
              fflush(stdout);
              isApiRequestReceived = true;
              apiRequestAction = ApiRequstAction::GO_TO_LIGHT_SETUP;
            }
            else if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_MEASURE) == 0)
            {
              printf("[API_FLOW] -> Branch: %s\n", SerialAPIRequestAction::API_REQUEST_MEASURE);
              fflush(stdout);

              cJSON *sensorItem = cJSON_GetObjectItemCaseSensitive(json, "sensorIndex");
              cJSON *curtainItem = cJSON_GetObjectItemCaseSensitive(json, "curtainMovement");

              if (cJSON_IsNumber(sensorItem) && cJSON_IsNumber(curtainItem))
              {
                if (sensorItem->valueint >= 0 && sensorItem->valueint < sensorsDataArraySize &&
                    curtainItem->valueint >= 0 && curtainItem->valueint <= 2)
                {
                  curSensorIndex = sensorItem->valueint;
                  curtainMovement = (CurtainMovement)curtainItem->valueint;
                  isApiRequestReceived = true;
                  apiRequestAction = ApiRequstAction::GO_TO_MEASURE;

                  printf("[API_FLOW] Measure parameters accepted\n");
                  fflush(stdout);
                }
                else
                {
                  printf("[API_FLOW] WARNING: Measure parameters OUT OF BOUNDS\n");
                  fflush(stdout);
                }
              }
              else
              {
                printf("[API_FLOW] WARNING: Measure parameters missing or invalid type\n");
                fflush(stdout);
              }
            }
            // --- CRUD API: ПОЛУЧЕНИЕ СПИСКА ЗАПИСЕЙ ---
            else if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_GET_RECORDS_LIST) == 0)
            {
              printf("[API_FLOW] -> Branch: %s\n", SerialAPIRequestAction::API_REQUEST_GET_RECORDS_LIST);
              fflush(stdout);

              cJSON *response = cJSON_CreateObject();
              cJSON_AddStringToObject(response, "cmd", SerialAPIRequestAction::API_REQUEST_GET_RECORDS_LIST);
              cJSON *records_array = cJSON_CreateArray();

              std::vector<int32_t> ids = storage.getAllValidRecordNumbers();

              printf("[API_FLOW] Found %d records in storage\n", ids.size());
              fflush(stdout);

              for (int32_t id : ids)
              {
                cJSON_AddItemToArray(records_array, cJSON_CreateNumber(id));
              }

              cJSON_AddStringToObject(response, "status", SerialAPIResponse::API_RESPONSE_STATUS_OK);
              cJSON_AddItemToObject(response, "records", records_array);

              char *json_str = cJSON_PrintUnformatted(response);
              printf("%s\n", json_str);
              fflush(stdout);

              printf("[API_FLOW] <- Response sent for %s\n", SerialAPIRequestAction::API_REQUEST_GET_RECORDS_LIST);
              fflush(stdout);

              free(json_str);
              cJSON_Delete(response);
            }
            // --- CRUD API: ЧТЕНИЕ КОНКРЕТНОЙ ЗАПИСИ ---
            else if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_GET_RECORD) == 0)
            {
              printf("[API_FLOW] -> Branch: %s\n", SerialAPIRequestAction::API_REQUEST_GET_RECORD);
              fflush(stdout);

              cJSON *record_num_item = cJSON_GetObjectItemCaseSensitive(json, "recordNumber");
              cJSON *response = cJSON_CreateObject();
              cJSON_AddStringToObject(response, "cmd", SerialAPIRequestAction::API_REQUEST_GET_RECORD);

              if (cJSON_IsNumber(record_num_item))
              {
                int target_id = record_num_item->valueint;

                printf("[API_FLOW] Requesting record ID: %d\n", target_id);
                fflush(stdout);

                StoredMeasuredResult rec = storage.getRecordByNumber(target_id);

                if (rec.recordNumber != -1) // Если найдено
                {
                  printf("[API_FLOW] Record %d found, packing JSON\n", target_id);
                  fflush(stdout);

                  cJSON_AddStringToObject(response, "status", SerialAPIResponse::API_RESPONSE_STATUS_OK);
                  cJSON *record_obj = cJSON_CreateObject();

                  cJSON_AddNumberToObject(record_obj, "recordNumber", rec.recordNumber);
                  cJSON_AddNumberToObject(record_obj, "sensor0Time", rec.sensor0Time);
                  cJSON_AddNumberToObject(record_obj, "sensor1Time", rec.sensor1Time);
                  cJSON_AddNumberToObject(record_obj, "curtain1spanAtime", rec.curtain1spanAtime);
                  cJSON_AddNumberToObject(record_obj, "curtain1spanAspeed", rec.curtain1spanAspeed);
                  cJSON_AddNumberToObject(record_obj, "curtain1TotalTime", rec.curtain1TotalTime);
                  cJSON_AddNumberToObject(record_obj, "curtain2spanAspeed", rec.curtain2spanAspeed);
                  cJSON_AddNumberToObject(record_obj, "curtain2spanAtime", rec.curtain2spanAtime);
                  cJSON_AddNumberToObject(record_obj, "curtain2TotalTime", rec.curtain2TotalTime);
                  cJSON_AddNumberToObject(record_obj, "slitWidthSensor0", rec.slitWidthSensor0);
                  cJSON_AddNumberToObject(record_obj, "slitWidthSensor1", rec.slitWidthSensor1);
                  cJSON_AddNumberToObject(record_obj, "slitWidthAverage", rec.slitWidthAverage);

                  cJSON_AddItemToObject(response, "record", record_obj);
                }
                else
                {
                  printf("[API_FLOW] WARNING: Record %d NOT FOUND\n", target_id);
                  fflush(stdout);

                  cJSON_AddStringToObject(response, "status", SerialAPIResponse::API_RESPONSE_STATUS_ERROR);
                  cJSON_AddStringToObject(response, "message", "Record not found");
                }
              }
              else
              {
                printf("[API_FLOW] WARNING: recordNumber is missing or not a number\n");
                fflush(stdout);
              }

              char *json_str = cJSON_PrintUnformatted(response);
              printf("%s\n", json_str);
              fflush(stdout);

              printf("[API_FLOW] <- Response sent for %s\n", SerialAPIRequestAction::API_REQUEST_GET_RECORD);
              fflush(stdout);

              free(json_str);
              cJSON_Delete(response);
            }
            // --- CRUD API: УДАЛЕНИЕ ЗАПИСИ ---
            else if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_DELETE_RECORD) == 0)
            {
              printf("[API_FLOW] -> Branch: %s\n", SerialAPIRequestAction::API_REQUEST_DELETE_RECORD);
              fflush(stdout);

              cJSON *record_num_item = cJSON_GetObjectItemCaseSensitive(json, "recordNumber");
              cJSON *response = cJSON_CreateObject();
              cJSON_AddStringToObject(response, "cmd", SerialAPIRequestAction::API_REQUEST_DELETE_RECORD);

              if (cJSON_IsNumber(record_num_item))
              {
                int target_id = record_num_item->valueint;

                printf("[API_FLOW] Attempting to delete ID: %d\n", target_id);
                fflush(stdout);

                bool success = storage.deleteRecordByNumber(target_id);

                if (success)
                {
                  printf("[API_FLOW] Record %d successfully deleted\n", target_id);
                  fflush(stdout);
                }
                else
                {
                  printf("[API_FLOW] WARNING: Failed to delete record %d (not found)\n", target_id);
                  fflush(stdout);
                }

                cJSON_AddStringToObject(response, "status", success ? 
                  SerialAPIResponse::API_RESPONSE_STATUS_OK : SerialAPIResponse::API_RESPONSE_STATUS_ERROR);
                if (!success)
                  cJSON_AddStringToObject(response, "message", "Record not found");
              }
              else
              {
                printf("[API_FLOW] WARNING: recordNumber is missing or not a number\n");
                fflush(stdout);
              }

              char *json_str = cJSON_PrintUnformatted(response);
              printf("%s\n", json_str);
              fflush(stdout);

              printf("[API_FLOW] <- Response sent for %s\n", SerialAPIRequestAction::API_REQUEST_DELETE_RECORD);
              fflush(stdout);

              free(json_str);
              cJSON_Delete(response);
            }
            // --- CRUD API: СОЗДАНИЕ И ИЗМЕНЕНИЕ ЗАПИСИ ---
            else if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_SAVE_RECORD) == 0)
            {
              printf("[API_FLOW] -> Branch: %s\n", SerialAPIRequestAction::API_REQUEST_SAVE_RECORD);
              fflush(stdout);

              cJSON *record_item = cJSON_GetObjectItemCaseSensitive(json, "record");
              cJSON *response = cJSON_CreateObject();
              cJSON_AddStringToObject(response, "cmd", SerialAPIRequestAction::API_REQUEST_SAVE_RECORD);

              if (cJSON_IsObject(record_item))
              {
                StoredMeasuredResult newRes;
                memset(&newRes, 0, sizeof(StoredMeasuredResult));
                newRes.isDeleted = false;

                cJSON *rn = cJSON_GetObjectItemCaseSensitive(record_item, "recordNumber");
                newRes.recordNumber = cJSON_IsNumber(rn) ? rn->valueint : 0;

                printf("[API_FLOW] Parsed recordNumber for saving: %ld\n", (long)newRes.recordNumber);
                fflush(stdout);

                cJSON *item;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "sensor0Time")) && cJSON_IsNumber(item))
                  newRes.sensor0Time = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "sensor1Time")) && cJSON_IsNumber(item))
                  newRes.sensor1Time = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "curtain1spanAtime")) && cJSON_IsNumber(item))
                  newRes.curtain1spanAtime = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "curtain1spanAspeed")) && cJSON_IsNumber(item))
                  newRes.curtain1spanAspeed = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "curtain1TotalTime")) && cJSON_IsNumber(item))
                  newRes.curtain1TotalTime = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "curtain2spanAspeed")) && cJSON_IsNumber(item))
                  newRes.curtain2spanAspeed = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "curtain2spanAtime")) && cJSON_IsNumber(item))
                  newRes.curtain2spanAtime = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "curtain2TotalTime")) && cJSON_IsNumber(item))
                  newRes.curtain2TotalTime = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "slitWidthSensor0")) && cJSON_IsNumber(item))
                  newRes.slitWidthSensor0 = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "slitWidthSensor1")) && cJSON_IsNumber(item))
                  newRes.slitWidthSensor1 = item->valuedouble;
                if ((item = cJSON_GetObjectItemCaseSensitive(record_item, "slitWidthAverage")) && cJSON_IsNumber(item))
                  newRes.slitWidthAverage = item->valuedouble;

                printf("[API_FLOW] Calling storage.saveOrUpdateRecord()...\n");
                fflush(stdout);

                int32_t savedId = storage.saveOrUpdateRecord(newRes);

                if (savedId != -1)
                {
                  printf("[API_FLOW] Success! Saved with ID: %ld\n", (long)savedId);
                  fflush(stdout);

                  cJSON_AddStringToObject(response, "status", SerialAPIResponse::API_RESPONSE_STATUS_OK);
                  cJSON_AddNumberToObject(response, "recordNumber", savedId);
                }
                else
                {
                  printf("[API_FLOW] ERROR: storage.saveOrUpdateRecord() returned error (-1)\n");
                  fflush(stdout);

                  cJSON_AddStringToObject(response, "status", SerialAPIResponse::API_RESPONSE_STATUS_ERROR);
                  cJSON_AddStringToObject(response, "message", "Failed to save record");
                }
              }
              else
              {
                printf("[API_FLOW] WARNING: 'record' object is missing in the payload\n");
                fflush(stdout);
              }

              char *json_str = cJSON_PrintUnformatted(response);
              printf("%s\n", json_str);
              fflush(stdout);

              printf("[API_FLOW] <- Response sent for %s\n", SerialAPIRequestAction::API_REQUEST_SAVE_RECORD);
              fflush(stdout);

              free(json_str);
              cJSON_Delete(response);
            }
            else if (strcmp(cmd_item->valuestring, SerialAPIRequestAction::API_REQUEST_FIRMWARE_UPDATE) == 0)
            {
              startFirmwareUpdate();
            }
            else
            {
              printf("[API_FLOW] WARNING: Unknown command: [%s]\n", cmd_item->valuestring);
              fflush(stdout);
            }
          }
          else
          {
            printf("[API_FLOW] WARNING: JSON has no 'cmd' field or it is not a string\n");
            fflush(stdout);
          }

          cJSON_Delete(json); // Освобождаем память после успешного парсинга
        }
        else
        {
          printf("[API_FLOW] ERROR: JSON parse error, invalid format.\n");
          printf("[API_FLOW] ERROR: Corrupted payload: %s\n", rxBuf);
          fflush(stdout);
        }

        // Очищаем буфер для следующей команды
        rxIdx = 0;
      }
    }
    else
    {
      // Накапливаем символы в буфер, защищаясь от переполнения
      if (rxIdx < API_BUF_SIZE - 1)
      {
        rxBuf[rxIdx++] = (char)c;
      }
      else
      {
        // Защита: если буфер переполнился, сбрасываем его
        printf("[API_FLOW] ERROR: !!! RX BUFFER OVERFLOW !!! String too long without '\\n'. Resetting.\n");
        fflush(stdout);
        rxIdx = 0;
      }
    }
  }
}