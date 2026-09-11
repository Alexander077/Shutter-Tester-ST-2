#pragma once

#include <string.h>
#include "lib/Common.h"

struct StoredMeasuredResult
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

  // Пользовательское имя записи (до RECORD_NAME_MAX_LEN символов,
  // пустая строка = имя не задано, показывается номер записи)
  char name[RECORD_NAME_BUF_SIZE];

  bool operator==(StoredMeasuredResult const &rhs) const
  {
    return memcmp(this, &rhs, sizeof(StoredMeasuredResult)) == 0;
  }
};