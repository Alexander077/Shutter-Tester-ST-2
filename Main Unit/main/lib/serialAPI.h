#pragma once

#include <Arduino.h>
#include "cJSON.h"
#include "esp_log.h"
#include "Common.h"
#include "StoredMeasuredResult.h"
#include "RecordsStorageManager.h" 

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

char *serialApiLightQualityStatusesStr[3] = {"LIGHT_QUALITY_UNKNOWN", "LIGHT_QUALITY_OK", "LIGHT_QUALITY_BAD"};

void serialApiTask(void *pvParameters)
{
  const size_t API_BUF_SIZE = 1024;
  char rx_buf[API_BUF_SIZE];
  setvbuf(stdin, NULL, _IONBF, 0);

  while (true)
  {
    if (fgets(rx_buf, API_BUF_SIZE, stdin) != NULL)
    {
      ESP_LOGI("API", "Received data: %s", rx_buf);
      cJSON *json = cJSON_Parse(rx_buf);

      if (json != NULL)
      {
        cJSON *cmd_item = cJSON_GetObjectItemCaseSensitive(json, "cmd");

        if (cJSON_IsString(cmd_item) && (cmd_item->valuestring != NULL))
        {
          if (strcmp(cmd_item->valuestring, "light_setup") == 0)
          {
            ESP_LOGI("API", "Command received: switch to light_setup");
            isApiRequestReceived = true;
            apiRequestAction = ApiRequstAction::GO_TO_LIGHT_SETUP;
          }
          else if (strcmp(cmd_item->valuestring, "measure") == 0)
          {
            ESP_LOGI("API", "Command received: switch to measure");
            cJSON *sensor_item = cJSON_GetObjectItemCaseSensitive(json, "sensor_index");
            cJSON *curtain_item = cJSON_GetObjectItemCaseSensitive(json, "curtain_movement");

            if (cJSON_IsNumber(sensor_item) && cJSON_IsNumber(curtain_item))
            {
              if (sensor_item->valueint >= 0 && sensor_item->valueint < sensorsDataArraySize &&
                  curtain_item->valueint >= 0 && curtain_item->valueint <= 2)
              {
                curSensorIndex = sensor_item->valueint;
                curtainMovement = (CurtainMovement)curtain_item->valueint;
                isApiRequestReceived = true;
                apiRequestAction = ApiRequstAction::GO_TO_MEASURE;
              }
            }
          }
          // --- CRUD API: ПОЛУЧЕНИЕ СПИСКА ЗАПИСЕЙ ---
          else if (strcmp(cmd_item->valuestring, "get_records_list") == 0)
          {
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "cmd", "get_records_list");
            cJSON *records_array = cJSON_CreateArray();

            std::vector<int32_t> ids = storage.getAllValidRecordNumbers();
            for (int32_t id : ids)
            {
              cJSON_AddItemToArray(records_array, cJSON_CreateNumber(id));
            }

            cJSON_AddStringToObject(response, "status", "ok");
            cJSON_AddItemToObject(response, "records", records_array);

            char *json_str = cJSON_PrintUnformatted(response);
            printf("%s\n", json_str);
            free(json_str);
            cJSON_Delete(response);
          }
          // --- CRUD API: ЧТЕНИЕ КОНКРЕТНОЙ ЗАПИСИ ---
          else if (strcmp(cmd_item->valuestring, "get_record") == 0)
          {
            cJSON *record_num_item = cJSON_GetObjectItemCaseSensitive(json, "recordNumber");
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "cmd", "get_record");

            if (cJSON_IsNumber(record_num_item))
            {
              StoredMeasuredResult rec = storage.getRecordByNumber(record_num_item->valueint);

              if (rec.recordNumber != -1) // Если найдено
              {
                cJSON_AddStringToObject(response, "status", "ok");
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
                cJSON_AddStringToObject(response, "status", "error");
                cJSON_AddStringToObject(response, "message", "Record not found");
              }
            }

            char *json_str = cJSON_PrintUnformatted(response);
            printf("%s\n", json_str);
            free(json_str);
            cJSON_Delete(response);
          }
          // --- CRUD API: УДАЛЕНИЕ ЗАПИСИ ---
          else if (strcmp(cmd_item->valuestring, "delete_record") == 0)
          {
            cJSON *record_num_item = cJSON_GetObjectItemCaseSensitive(json, "recordNumber");
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "cmd", "delete_record");

            if (cJSON_IsNumber(record_num_item))
            {
              bool success = storage.deleteRecordByNumber(record_num_item->valueint);
              cJSON_AddStringToObject(response, "status", success ? "ok" : "error");
              if (!success)
                cJSON_AddStringToObject(response, "message", "Record not found");
            }

            char *json_str = cJSON_PrintUnformatted(response);
            printf("%s\n", json_str);
            free(json_str);
            cJSON_Delete(response);
          }
          // --- CRUD API: СОЗДАНИЕ И ИЗМЕНЕНИЕ ЗАПИСИ ---
          else if (strcmp(cmd_item->valuestring, "save_record") == 0)
          {
            cJSON *record_item = cJSON_GetObjectItemCaseSensitive(json, "record");
            cJSON *response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "cmd", "save_record");

            if (cJSON_IsObject(record_item))
            {
              StoredMeasuredResult newRes;
              memset(&newRes, 0, sizeof(StoredMeasuredResult));
              newRes.isDeleted = false;

              cJSON *rn = cJSON_GetObjectItemCaseSensitive(record_item, "recordNumber");
              newRes.recordNumber = cJSON_IsNumber(rn) ? rn->valueint : 0;

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

              int32_t savedId = storage.saveOrUpdateRecord(newRes);

              if (savedId != -1)
              {
                cJSON_AddStringToObject(response, "status", "ok");
                cJSON_AddNumberToObject(response, "recordNumber", savedId);
              }
              else
              {
                cJSON_AddStringToObject(response, "status", "error");
                cJSON_AddStringToObject(response, "message", "Failed to save record");
              }
            }

            char *json_str = cJSON_PrintUnformatted(response);
            printf("%s\n", json_str);
            free(json_str);
            cJSON_Delete(response);
          }
        }
        cJSON_Delete(json);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}