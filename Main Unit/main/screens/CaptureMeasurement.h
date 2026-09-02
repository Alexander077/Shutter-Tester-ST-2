#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "lib/ADC.h"
#include "lib/serialAPI.h"
#include <inttypes.h>

#define EDGE_BUFFER_SIZE 5000
#define NOISE_THRESHOLD 1000 // Порог начала импульса
#define ONE_ADC_CONVERSION_TIME_US 2.5

// --- Добавленные состояния и константы для конечного автомата ---
enum class SensorMesuringState
{
  Idle,
  PulseActive,
  MeasurementFinished
};

struct RawMeasurementCapture
{
  bool isCancelled;
  uint32_t sensor0PulseWidthUs;
  uint32_t sensor1PulseWidthUs;
  uint32_t curtain1TimeUs;
  uint32_t curtain2TimeUs;
};

// Performs a single ADC capture showing the measuring screen. Used by both the
// single measurement flow and the measurement series flow.
RawMeasurementCapture captureMeasurement(const char *title, const char *subtitle, const char *subtitle2 = "")
{
  RawMeasurementCapture captureRes = {false, 0, 0, 0, 0};

  printf("Starting ADC continuous reading...\n");

  vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
  ESP_ERROR_CHECK(adc_continuous_start(handle));

  uint32_t waitForSensorTime = 0;

  uint32_t curtain1ADCSamplesCounter = 0;
  uint32_t curtain2ADCSamplesCounter = 0;

  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextSize(2);
  drawStringHCentered(title, 40);
  display->setTextSize(1);
  drawStringHCentered(subtitle, 60);

  if (subtitle2[0] != '\0')
  {
    drawStringHCentered(subtitle2, 75);
  }

  esp_err_t adcReadRes;
  uint32_t retNum = 0;
  uint8_t result[READ_LEN] = {0};
  memset(result, 0xcc, READ_LEN);

  // --- Переменные для расчета длительности импульса ---
  static SensorMesuringState sensor0State = SensorMesuringState::Idle;
  static uint32_t sensor0SampleCount = 0;
  static uint16_t sensor0EdgeBuffer[EDGE_BUFFER_SIZE] = {0};
  sensor0State = SensorMesuringState::Idle;
  sensor0SampleCount = 0;
  sensor0Max = 0;
  uint32_t sensor0PulseWidthUs = 0;
  memset(sensor0EdgeBuffer, 0, sizeof(sensor0EdgeBuffer));

  static SensorMesuringState sensor1State = SensorMesuringState::Idle;
  static uint32_t sensor1SampleCount = 0;
  static uint16_t sensor1EdgeBuffer[EDGE_BUFFER_SIZE] = {0};
  sensor1State = SensorMesuringState::Idle;
  sensor1SampleCount = 0;
  sensor1Max = 0;
  memset(sensor1EdgeBuffer, 0, sizeof(sensor1EdgeBuffer));
  uint32_t sensor1PulseWidthUs = 0;

  // bool edgeBufferOverflow = false;

  // static const uint16_t waveformRecordbufferLength = 5000;
  // static uint16_t waveformRecordbuffer[waveformRecordbufferLength] = {0};
  // static uint16_t waveformRecordbufferIndex = 0;
  // waveformRecordbufferIndex = 0;
  // memset(waveformRecordbuffer, 0, sizeof(waveformRecordbuffer));

  bool valWasZero = false;

  while (true)
  {
    if (isApiRequestReceived) // ВЫХОД ДЛЯ API с остановкой АЦП
    {
      ESP_ERROR_CHECK(adc_continuous_stop(handle));
      captureRes.isCancelled = true;
      return captureRes;
    }

    adcReadRes = adc_continuous_read(handle, result, READ_LEN, &retNum, UINT32_MAX);

    if (adcReadRes == ESP_OK)
    {
      // digitalWrite(TEST_PIN_2, HIGH);

      // Быстрый ручной парсинг: каждый сэмпл — 2 байта (SOC_ADC_DIGI_RESULT_BYTES = 2)
      // Формат ESP32 (type1): [15:12] = канал, [11:0] = данные
      uint16_t *raw = (uint16_t *)result;
      uint32_t numSamples = retNum / SOC_ADC_DIGI_RESULT_BYTES;

      for (uint32_t i = 0; i < numSamples; i++)
      {
        uint16_t ch = (raw[i] >> 12) & 0xF; // номер канала АЦП
        uint16_t val = raw[i] & 0xFFF;      // значение АЦП (0..4095 для 12 бит)

        if (ch == ADC_CHANNEL_6)
        {
          // Реализация конечного автомата для ADC_CHANNEL_6
          if (sensor0State == SensorMesuringState::Idle)
          {
            if (val == 0)
            {
              valWasZero = true;
            }

            if (valWasZero && val > NOISE_THRESHOLD)
            {
              // Начало импульса
              sensor0State = SensorMesuringState::PulseActive;
              sensor0Max = val;
              sensor0SampleCount = 0;
              sensor0EdgeBuffer[0] = val;
              // waveformRecordbuffer[waveformRecordbufferIndex++] = val;
            }
          }
          else if (sensor0State == SensorMesuringState::PulseActive)
          {
            sensor0SampleCount++;

            // Сохраняем начальные сэмплы для ретроспективного поиска середины фронта
            if (sensor0SampleCount < EDGE_BUFFER_SIZE)
            {
              sensor0EdgeBuffer[sensor0SampleCount] = val;
            }

            // // waveformRecordbuffer[waveformRecordbufferIndex++] = val;

            // Постоянно обновляем значение вершины
            if (val > sensor0Max)
            {
              sensor0Max = val;
            }

            // Ловим спад: текущее значение упало ниже половины от найденного максимума
            if (val < sensor0Max / 2)
            {
              sensor0State = SensorMesuringState::MeasurementFinished;
              // ESP_LOGI(TAG, "Measured pulse: %" PRIu32 " us (Peak ADC: %u, Half-Max: %u)", width_us, max_val, half_max);
            }
          }
        }
        else // ch == ADC_CHANNEL_7
        {
          // Реализация конечного автомата для ADC_CHANNEL_6
          if (sensor1State == SensorMesuringState::Idle)
          {
            if (val > NOISE_THRESHOLD)
            {
              // Начало импульса
              sensor1State = SensorMesuringState::PulseActive;
              sensor1Max = val;
              sensor1SampleCount = 0;
              sensor1EdgeBuffer[0] = val;
            }
          }
          else if (sensor1State == SensorMesuringState::PulseActive)
          {
            sensor1SampleCount++;

            // Сохраняем начальные сэмплы для ретроспективного поиска середины фронта
            if (sensor1SampleCount < EDGE_BUFFER_SIZE)
            {
              sensor1EdgeBuffer[sensor1SampleCount] = val;
            }

            // Постоянно обновляем значение вершины
            if (val > sensor1Max)
            {
              sensor1Max = val;
            }

            // Ловим спад: текущее значение упало ниже половины от найденного максимума
            if (val < sensor1Max / 2)
            {
              sensor1State = SensorMesuringState::MeasurementFinished;
            }
          }
        }

        // Датчик "открыт", если его автомат покинул Idle (автомат не возвращается в Idle)
        bool isSensor0Opened = sensor0State != SensorMesuringState::Idle;
        bool isSensor1Opened = sensor1State != SensorMesuringState::Idle;
        // Датчик "закрыт", если зафиксирован спад импульса (WaitForZero или MeasurementFinished)
        bool isSensor0Closed = sensor0State >= SensorMesuringState::MeasurementFinished;
        bool isSensor1Closed = sensor1State >= SensorMesuringState::MeasurementFinished;

        // Считаем время, пока 1-я шторка открыла ровно один из двух датчиков
        if (isSensor0Opened != isSensor1Opened)
        {
          curtain1ADCSamplesCounter++;
        }

        // Считаем время, пока 2-я шторка закрыла ровно один из двух датчиков
        if (isSensor0Closed != isSensor1Closed)
        {
          curtain2ADCSamplesCounter++;
        }
      }
    }
    else if (adcReadRes == ESP_ERR_TIMEOUT)
    {
      // We try to read `READ_LEN` until API returns timeout, which means there's no available data
      // ESP_LOGW("", "ADC timed out");
      break;
    }

    // digitalWrite(TEST_PIN_2, LOW);

    if (button.isClicked())
    {
      ESP_ERROR_CHECK(adc_continuous_stop(handle));
      captureRes.isCancelled = true;
      return captureRes;
    }
    else if (sensor0State == SensorMesuringState::MeasurementFinished ||
             sensor1State == SensorMesuringState::MeasurementFinished) // Check if at least one sensor has data
    {
      // if (edgeBufferOverflow)
      // {
      // 	// printf("Edge buffer overflow\n");//TODO: add error message screen
      // 	halt("Edge buffer overflow");
      // }

      if (waitForSensorTime == 0)
      {
        waitForSensorTime = millis();
      }

      // wait for other sensor to get data
      if (millis() - waitForSensorTime > 500)
      {
        ESP_ERROR_CHECK(adc_continuous_stop(handle));

        auto calculatePulseWidth = [](uint32_t sampleCount, uint16_t maxValue, const uint16_t *edgeBuffer) -> uint32_t
        {
          uint32_t startSample = 0;
          uint16_t halfMax = maxValue / 2;

          // Ищем точку на начальном склоне (в буфере), которая ближе всего пересекла half_max
          uint32_t limit = (sampleCount < EDGE_BUFFER_SIZE) ? sampleCount : EDGE_BUFFER_SIZE;

          for (uint32_t j = 0; j < limit; j++)
          {
            if (edgeBuffer[j] >= halfMax)
            {
              startSample = j;
              break;
            }
          }

          // Считаем длительность
          uint32_t widthSamples = sampleCount - startSample;
          return widthSamples * ONE_ADC_CONVERSION_TIME_US;
        };

        if (sensor0State == SensorMesuringState::MeasurementFinished)
        {
          sensor0PulseWidthUs = calculatePulseWidth(sensor0SampleCount, sensor0Max, sensor0EdgeBuffer);
        }

        if (sensor1State == SensorMesuringState::MeasurementFinished)
        {
          sensor1PulseWidthUs = calculatePulseWidth(sensor1SampleCount, sensor1Max, sensor1EdgeBuffer);
        }

        printf("s1 time: %" PRIu32 "\n", sensor0PulseWidthUs);
        printf("s2 time: %" PRIu32 "\n", sensor1PulseWidthUs);

        printf("s1 max: %" PRIu32 "\n", (uint32_t)sensor0Max);
        printf("s2 max: %" PRIu32 "\n", (uint32_t)sensor1Max);

        printf("c1 time: %" PRIu32 "\n", (uint32_t)((curtain1ADCSamplesCounter / 2) * ONE_ADC_CONVERSION_TIME_US));
        printf("c2 time: %" PRIu32 "\n", (uint32_t)((curtain2ADCSamplesCounter / 2) * ONE_ADC_CONVERSION_TIME_US));

        // for (size_t i = 0; i < waveformRecordbufferIndex; i++)
        // {
        // 	printf("%u\n", waveformRecordbuffer[i]);
        // }

        fflush(stdout);

        // SERIAL_API_DEBUG_PRINT("ADC conversions taken for sensor 2: %" PRIu32, sensor2ADCSamplesCounter);

        captureRes.sensor0PulseWidthUs = sensor0PulseWidthUs;
        captureRes.sensor1PulseWidthUs = sensor1PulseWidthUs;
        captureRes.curtain1TimeUs = (curtain1ADCSamplesCounter / 2) * ONE_ADC_CONVERSION_TIME_US;
        captureRes.curtain2TimeUs = (curtain2ADCSamplesCounter / 2) * ONE_ADC_CONVERSION_TIME_US;

        return captureRes;
      }
    }
  }

  return captureRes;
}