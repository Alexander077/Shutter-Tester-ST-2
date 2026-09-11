#pragma once

#include <string.h>
#include "lib/Common.h"
#include "lib/MeasuredResult.h"

// Легаси-форматы записей (без поля 'name'), сохраненные только для однократной
// миграции существующих файлов records.bin / seriesRecords.bin при обновлении
// прошивки (см. RecordsManagementCommon.h). Эти структуры обязаны оставаться
// байт-в-байт идентичными форматам, использовавшимся до добавления поля 'name'.

struct OldStoredMeasuredResult
{
  int32_t recordNumber;
  bool isDeleted;

  double sensor0Time; // In milliseconds
  double sensor1Time; // In milliseconds

  double curtain1spanAtime ; // In milliseconds
  double curtain1spanAspeed; // In meters per second
  double curtain1TotalTime; // In milliseconds

  double curtain2spanAtime; // In milliseconds
  double curtain2spanAspeed; // In meters per second
  double curtain2TotalTime; // In milliseconds

  double slitWidthSensor0; // in mm
  double slitWidthSensor1; // in mm
  double slitWidthAverage; // in mm

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