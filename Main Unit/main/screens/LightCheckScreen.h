#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "lib/ADC.h"
#include "lib/serialAPI.h"
#include "cJSON.h"
#include <inttypes.h>

enum class LightQualityStatus
{
  UNKNOWN,
  OK,
  BAD,
};

void drawLightCheckScreen()
{
  uint32_t sensor1TotalADCSamplesCounter = 0;
  uint32_t sensor1OpenADCSamplesCounter = 0;
  uint32_t sensor2TotalADCSamplesCounter = 0;
  uint32_t sensor2OpenADCSamplesCounter = 0;
  const uint32_t displayUpdateSamplesCount = 40000; // 200ms interval

  esp_err_t adcReadRes;
  uint32_t retNum = 0;
  uint8_t result[READ_LEN] = {0};
  memset(result, 0xcc, READ_LEN);

  const char *lightQualityBrightnessLabel = "Light quality";
  char *lightQualityStatusesStr[3] = {"Unknown", "Ok", "Bad"};
  display->fillScreen(BLACK);

  uint8_t barX = 65;
  uint8_t barY = 52;
  drawSensorSignalLevelBar(barX, barY, 1, 0); // draw zero value to force ful bar visibility
  drawSensorSignalLevelBar(barX, barY, 2, 0); // draw zero value to force ful bar visibility

  while (true)
  {
    if (isApiRequestReceived)
    {
      if (apiRequestAction == ApiRequestAction::GO_TO_LIGHT_SETUP)
      {
        isApiRequestReceived = false; // Сбрасываем, если мы уже здесь
      }
      else
      {
        return;
      }
    }

    sensor0Max = 0;
    sensor1Max = 0;
    sensor1TotalADCSamplesCounter = 0;
    sensor2TotalADCSamplesCounter = 0;
    sensor1OpenADCSamplesCounter = 0;
    sensor2OpenADCSamplesCounter = 0;
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while (sensor1TotalADCSamplesCounter < displayUpdateSamplesCount)
    {
      if (isApiRequestReceived && apiRequestAction != ApiRequestAction::GO_TO_LIGHT_SETUP)
      {
        ESP_ERROR_CHECK(adc_continuous_stop(handle));
        return;
      }

      adcReadRes = adc_continuous_read(handle, result, READ_LEN, &retNum, UINT32_MAX);

      if (adcReadRes == ESP_OK)
      {
        // Быстрый ручной парсинг: каждый сэмпл — 2 байта (SOC_ADC_DIGI_RESULT_BYTES = 2)
        // Формат ESP32 (type1): [15:12] = канал, [11:0] = данные
        uint16_t *raw = (uint16_t *)result;
        uint32_t num_samples = retNum / SOC_ADC_DIGI_RESULT_BYTES;

        for (uint32_t i = 0; i < num_samples; i++)
        {
          uint16_t ch = (raw[i] >> 12) & 0xF; // номер канала АЦП
          uint16_t val = raw[i] & 0xFFF;      // значение АЦП (0..4095 для 12 бит)

          if (ch == ADC_CHANNEL_6) // senosor 1
          {
            if (val > sensor0Max)
            {
              sensor0Max = val;
            }

            if (val > SHUTTER_OPEN_LEVEL)
            {
              sensor1OpenADCSamplesCounter++;
            }

            sensor1TotalADCSamplesCounter++;
          }
          else // sensor 2
          {
            if (val > sensor1Max)
            {
              sensor1Max = val;
            }

            if (val > SHUTTER_OPEN_LEVEL)
            {
              sensor2OpenADCSamplesCounter++;
            }

            sensor2TotalADCSamplesCounter++;
          }
        }
      }
      else if (adcReadRes == ESP_ERR_TIMEOUT)
      {
        // We try to read `READ_LEN` until API returns timeout, which means there's no available data
        ESP_LOGW("", "ADC timed out");
        break;
      }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_LOGI("", "Sen 1 tot. samp.: %" PRIu32 "(Open samp.: %" PRIu32 ") , sen 2 tot. samp.: %" PRIu32 "(Open samp.: %" PRIu32 "), sen 1 max: %" PRIu16 ", sen 2 max: %" PRIu16,
             sensor1TotalADCSamplesCounter, sensor1OpenADCSamplesCounter, sensor2TotalADCSamplesCounter, sensor2OpenADCSamplesCounter, sensor0Max, sensor1Max);

#pragma region // display interaction

    static LightQualityStatus oldLightQualityStatus = LightQualityStatus::UNKNOWN;
    LightQualityStatus resLightQualityStatus = LightQualityStatus::UNKNOWN;
    LightQualityStatus sen1LightQualityStatus = LightQualityStatus::UNKNOWN;
    LightQualityStatus sen2LightQualityStatus = LightQualityStatus::UNKNOWN;

    if (sensor0Max > SHUTTER_OPEN_LEVEL && sensor0Max < MAX_ALLOWED_SIGNAL_LEVEL)
    {
      if (sensor1OpenADCSamplesCounter < sensor1TotalADCSamplesCounter)
      {
        sen1LightQualityStatus = LightQualityStatus::BAD;
      }
      else
      {
        sen1LightQualityStatus = LightQualityStatus::OK;
      }
    }

    if (sensor1Max > SHUTTER_OPEN_LEVEL && sensor1Max < MAX_ALLOWED_SIGNAL_LEVEL)
    {
      if (sensor2OpenADCSamplesCounter < sensor2TotalADCSamplesCounter)
      {
        sen2LightQualityStatus = LightQualityStatus::BAD;
      }
      else
      {
        sen2LightQualityStatus = LightQualityStatus::OK;
      }
    }

    if (sen1LightQualityStatus == LightQualityStatus::BAD ||
        sen2LightQualityStatus == LightQualityStatus::BAD)
    {
      resLightQualityStatus = LightQualityStatus::BAD;
    }
    else if (sen1LightQualityStatus == LightQualityStatus::OK &&
             sen2LightQualityStatus == LightQualityStatus::OK)
    {
      resLightQualityStatus = LightQualityStatus::OK;
    }
    else if (sen1LightQualityStatus == LightQualityStatus::OK &&
             sen2LightQualityStatus == LightQualityStatus::UNKNOWN)
    {
      resLightQualityStatus = LightQualityStatus::OK;
    }
    else if (sen2LightQualityStatus == LightQualityStatus::OK &&
             sen1LightQualityStatus == LightQualityStatus::UNKNOWN)
    {
      resLightQualityStatus = LightQualityStatus::OK;
    }
    else
    {
      resLightQualityStatus = LightQualityStatus::UNKNOWN;
    }

    uint8_t x = 16;
    uint8_t y = 20;

    drawStringHCentered("Light quality", y);
    y += 30;

    display->setTextSize(2);

    if (oldLightQualityStatus != resLightQualityStatus)
    {
      display->setTextColor(BLACK);
      drawStringHCentered(lightQualityStatusesStr[(int8_t)oldLightQualityStatus], y);
      oldLightQualityStatus = resLightQualityStatus;
    }

    uint16_t lightQualityStatusTextColors[3] = {LIGHTGREY, GREEN, RED};

    display->setCursor(x, y);
    display->setTextColor(lightQualityStatusTextColors[(int8_t)resLightQualityStatus]);
    drawStringHCentered(lightQualityStatusesStr[(int8_t)resLightQualityStatus], y);

    barX = 65;
    barY = 52;

    display->setTextColor(WHITE);
    display->setTextSize(1);
    drawSensorSignalLevelBar(barX, barY, 1, sensor0Max);
    drawSensorSignalLevelBar(barX, barY, 2, sensor1Max);

#pragma endregion // display interaction

    // === ГЕНЕРАЦИЯ JSON ДЛЯ API ===
    cJSON *json = cJSON_CreateObject();

    if (json != NULL)
    {
      cJSON_AddStringToObject(json, "cmd", SerialAPIRequestAction::API_REQUEST_LIGHT_SETUP);
      cJSON_AddNumberToObject(json, "sensor1Level", (int)((float)sensor0Max / MAX_SIGNAL_LEVEL * 100)); // in percents
      cJSON_AddNumberToObject(json, "sensor2Level", (int)((float)sensor1Max / MAX_SIGNAL_LEVEL * 100)); // in percents

      const char *s1Status = (sensor0Max <= MIN_ALLOWED_SIGNAL_LEVEL) ? SerialAPILightStatus::LIGHT_STATUS_TOO_DIMM : ((sensor0Max > MAX_ALLOWED_SIGNAL_LEVEL) ? SerialAPILightStatus::LIGHT_STATUS_TOO_BRIGHT : SerialAPILightStatus::LIGHT_STATUS_OK);

      const char *s2Status = (sensor1Max <= MIN_ALLOWED_SIGNAL_LEVEL) ? SerialAPILightStatus::LIGHT_STATUS_TOO_DIMM : ((sensor1Max > MAX_ALLOWED_SIGNAL_LEVEL) ? SerialAPILightStatus::LIGHT_STATUS_TOO_BRIGHT : SerialAPILightStatus::LIGHT_STATUS_OK);

      cJSON_AddStringToObject(json, "sensor1Status", s1Status);
      cJSON_AddStringToObject(json, "sensor2Status", s2Status);
      cJSON_AddStringToObject(json, "lightQuality", serialApiLightQualityStatusesStr[(int8_t)resLightQualityStatus]);

      char *json_str = cJSON_PrintUnformatted(json);
      safePrintJson(json_str);
      free(json_str);

      cJSON_Delete(json);
    }
    // ==============================

    if (button.isClicked())
    {
      return;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}