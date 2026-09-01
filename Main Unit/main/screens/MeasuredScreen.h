#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/computeMeasuredResult.h"
#include "screens/MeasuredResult.h"
#include "screens/MeasurementSaveScreen.h"
#include "lib/serialAPI.h"
#include "cJSON.h"

void drawMeasuredScreen(uint32_t rawSensor0TimeTakenUs, uint32_t rawSensor1TimeTakenUs, uint32_t curtain1TimeUs, uint32_t curtain2TimeUs)
{
  // ---------------------------------------------------------------
  // Shutter result screen
  //   shutter time im us/ms (e.g. 1.55 ms)
  //   shutter speed in fraction of a sec. (e.g. 1/750 sec)
  //   Distance from closest shutter speeds graphically

  // Total result screen
  //   1 to 2 sensor first curtain travel time in ms
  //   1 to 2 sensor second curtain travel time in ms

  //   1 to 2 sensor first curtain speed in m/s
  //   1 to 2 sensor second curtain speed in m/s

  //   1-st curtain est. total travel time in ms
  //   2-st curtain est. total travel time in ms

  //   Slit size for first and second sensor in mm (e.g. 1.5mm)
  // ---------------------------------------------------------------

  ComputedMeasurement computed;
  computeMeasuredResult(rawSensor0TimeTakenUs, rawSensor1TimeTakenUs, curtain1TimeUs, curtain2TimeUs, computed);

  MeasuredResult &res = computed.res;
  bool sensor0DataOk = computed.sensor0DataOk;
  bool sensor1DataOk = computed.sensor1DataOk;

  // === ГЕНЕРАЦИЯ JSON ДЛЯ API ===
  if (apiRequestAction == ApiRequestAction::GO_TO_MEASURE)
  {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "cmd", SerialAPIRequestAction::API_REQUEST_MEASURE);

    if (res.sensor0Time != MEASUREMENTS_IS_INVALID_VAL || res.sensor1Time != MEASUREMENTS_IS_INVALID_VAL)
    {
      cJSON_AddStringToObject(json, "status", SerialAPIResponse::API_RESPONSE_STATUS_OK);

      // Записываем данные сенсоров
      cJSON_AddNumberToObject(json, "sensor0Time", res.sensor0Time);
      cJSON_AddNumberToObject(json, "sensor1Time", res.sensor1Time);

      // Записываем данные по шторкам (скорости и время)
      cJSON_AddNumberToObject(json, "curtain1spanAtime", res.curtain1spanAtime);
      cJSON_AddNumberToObject(json, "curtain1spanAspeed", res.curtain1spanAspeed);
      cJSON_AddNumberToObject(json, "curtain2spanAtime", res.curtain2spanAtime);
      cJSON_AddNumberToObject(json, "curtain2spanAspeed", res.curtain2spanAspeed);

      // Записываем данные ширины щели
      cJSON_AddNumberToObject(json, "slitWidthSensor0", res.slitWidthSensor0);
      cJSON_AddNumberToObject(json, "slitWidthSensor1", res.slitWidthSensor1);
      cJSON_AddNumberToObject(json, "slitWidthAverage", res.slitWidthAverage);
    }
    else
    {
      cJSON_AddStringToObject(json, "status", SerialAPIResponse::API_RESPONSE_STATUS_ERROR);
      cJSON_AddStringToObject(json, "error", "Invalid measurement data");
    }

    char *json_str = cJSON_PrintUnformatted(json);
    safePrintJson(json_str);
    free(json_str);

    cJSON_Delete(json);
    return;
  }

  MeasurementSaveScreenResult saveScreenRes = MeasurementSaveScreenResult::OK;

  do
  {
    renderMeasuredResult(res);
    if (isApiRequestReceived)
      return; // ВЫХОД ДЛЯ API

    if (sensor0DataOk || sensor1DataOk)
    {
      saveScreenRes = drawMeasurementSaveScreen(res);
    }

  } while (saveScreenRes == MeasurementSaveScreenResult::CANCEL && !isApiRequestReceived);
}