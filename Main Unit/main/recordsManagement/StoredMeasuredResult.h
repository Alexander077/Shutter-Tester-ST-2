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

  // User-editable record name, always '\0'-terminated. Appended at the end
  // so the layout of all previous fields is unchanged.
  char name[RECORD_NAME_MAX_LEN + 1];

  bool operator==(StoredMeasuredResult const &rhs) const
  {
    return memcmp(this, &rhs, sizeof(StoredMeasuredResult)) == 0;
  }
};