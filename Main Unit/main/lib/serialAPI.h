#pragma once

#include <Arduino.h>
#include "cJSON.h"
#include "esp_log.h"
#include "Common.h"

// Глобальный флаг для запроса API
volatile bool isApiRequestReceived = false;

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
  const size_t API_BUF_SIZE = 512;
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

            // Пытаемся получить параметры из JSON
            cJSON *sensor_item = cJSON_GetObjectItemCaseSensitive(json, "sensor_index");
            cJSON *curtain_item = cJSON_GetObjectItemCaseSensitive(json, "curtain_movement");

            // Проверяем, что параметры переданы и это числа
            if (cJSON_IsNumber(sensor_item) && cJSON_IsNumber(curtain_item))
            {
              // Желательно добавить валидацию, чтобы индекс не вышел за пределы массива
              if (sensor_item->valueint >= 0 && sensor_item->valueint < sensorsDataArraySize &&
                  curtain_item->valueint >= 0 && curtain_item->valueint <= 2)
              {
                // Устанавливаем глобальные переменные
                curSensorIndex = sensor_item->valueint;
                curtainMovement = (CurtainMovement)curtain_item->valueint;

                isApiRequestReceived = true;
                apiRequestAction = ApiRequstAction::GO_TO_MEASURE;
              }
              else
              {
                ESP_LOGE("API", "Measure command parameters are out of range");
              }
            }
            else
            {
              ESP_LOGE("API", "Measure command missing sensor_index or curtain_movement");
            }
          }
        }

        cJSON_Delete(json); // Обязательно освобождаем память
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}