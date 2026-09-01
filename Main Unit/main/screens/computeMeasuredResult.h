#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"

struct CurtainTimings
{
  double curtain1spanAtime; // In microseconds
  double curtain2spanAtime; // In microseconds
};

struct SensorUnitData
{
  SensorType Type;
  double FrameWidth;               // in millimiters
  double FrameHeight;              // in millimiters
  double HorisontalSensorDistance; // in millimiters
  double VerticalSensorDistance;   // in millimiters
};

const SensorUnitData sensorsData[sensorsDataArraySize] = 
{
	{
		SensorType::Frame35mm,
		36,
		24,
		16.0,
		10.6,
	},
	{
		SensorType::Frame6x45,
		60,
		45,
		16.0,
		10.6,
	},
	{
		SensorType::Frame6x6,
		60,
		60,
		16.0,
		10.6,
	},
	{
		SensorType::Frame6x7,
		70,
		60,
		16.0,
		10.6,
	}
};

double getCurtainSpeed(long time, double distance)
{
  double curtainSpeedMmPerUs = distance / time;
  double curtainSpeedInMmPerS = curtainSpeedMmPerUs * (double)US_IN_SECOND;
  double curtainSpanASpeedInMPerS = curtainSpeedInMmPerS / (double)MM_IN_M;
  return curtainSpanASpeedInMPerS;
}

void calculateResults(MeasuredResult &res, const CurtainTimings curtainTimings, double sensorDistance, double frameSize, double sensor0time, double sensor1time)
{
  double frameSizeInMeters = frameSize / (double)MM_IN_M;
  // First curtain
  res.curtain1spanAspeed = getCurtainSpeed(curtainTimings.curtain1spanAtime, sensorDistance);
  res.curtain1spanAtime = curtainTimings.curtain1spanAtime / US_IN_MILLISECOND;
  res.curtain1TotalTime = frameSizeInMeters / (res.curtain1spanAspeed / 1000.0);

  // Second curtain
  res.curtain2spanAspeed = getCurtainSpeed(curtainTimings.curtain2spanAtime, sensorDistance);
  res.curtain2spanAtime = curtainTimings.curtain2spanAtime / US_IN_MILLISECOND;
  res.curtain2TotalTime = frameSizeInMeters / (res.curtain2spanAspeed / 1000.0);

  // Slit width
  double estSlitSpeed = sensorDistance / ((curtainTimings.curtain1spanAtime + curtainTimings.curtain2spanAtime) / 2.0); // in mm per microsecond
  res.slitWidthSensor0 = estSlitSpeed * sensor0time;                                                                    // in mm
  res.slitWidthSensor1 = estSlitSpeed * sensor1time;                                                                    // in mm
  res.slitWidthAverage = (res.slitWidthSensor0 + res.slitWidthSensor1) / 2.0;                                           // in mm
}

// Computes MeasuredResult from raw capture data. Kept separate from the screen
// rendering so the measurement series flow can reuse it.
void computeMeasuredResult(uint32_t rawSensor0TimeTakenUs, uint32_t rawSensor1TimeTakenUs, uint32_t curtain1TimeUs, uint32_t curtain2TimeUs, ComputedMeasurement &out)
{
  out.sensor0DataOk = false;
  out.sensor1DataOk = false;

  MeasuredResult &res = out.res;
  res.selectedCurtainMovement = curtainMovement;

  if (sensor0Max <= MIN_ALLOWED_SIGNAL_LEVEL)
  {
    res.sensor0Time = SENSOR_LIGHT_IS_TOO_DIM;
  }
  else if (sensor0Max > MAX_ALLOWED_SIGNAL_LEVEL)
  {
    res.sensor0Time = SENSOR_LIGHT_IS_TOO_BRIGHT;
  }
  else
  {
    // correctedSensor0TimeTakenUs = getCorrectedSensorValue(rawSensor0TimeTakenUs, sensor0Max); // in microseconds
    // res.sensor0Time = correctedSensor0TimeTakenUs / US_IN_MILLISECOND;												// in milliseconds
    res.sensor0Time = (double)rawSensor0TimeTakenUs / US_IN_MILLISECOND; // in milliseconds, no correction for new algorythm

    if (res.sensor0Time < TOO_SHORT_SENSOR_TIME)
    {
      res.sensor0Time = SENSOR_TIME_IS_TOO_SHORT;
    }
    else
    {
      out.sensor0DataOk = true;
    }
  }

  if (sensor1Max <= MIN_ALLOWED_SIGNAL_LEVEL)
  {
    res.sensor1Time = SENSOR_LIGHT_IS_TOO_DIM;
  }
  else if (sensor1Max > MAX_ALLOWED_SIGNAL_LEVEL)
  {
    res.sensor1Time = SENSOR_LIGHT_IS_TOO_BRIGHT;
  }
  else
  {
    // correctedSensor1TimeTakenUs = getCorrectedSensorValue(rawSensor1TimeTakenUs, sensor1Max); // in microseconds
    // res.sensor1Time = correctedSensor1TimeTakenUs / US_IN_MILLISECOND;												// in milliseconds
    res.sensor1Time = (double)rawSensor1TimeTakenUs / US_IN_MILLISECOND; // in milliseconds, no correction for new algorythm

    if (res.sensor1Time < TOO_SHORT_SENSOR_TIME)
    {
      res.sensor1Time = SENSOR_TIME_IS_TOO_SHORT;
    }
    else
    {
      out.sensor1DataOk = true;
    }
  }

  SERIAL_API_DEBUG_PRINT("Sensor 0 max: %" PRIu16, sensor0Max);
  SERIAL_API_DEBUG_PRINT("Sensor 1 max: %" PRIu16, sensor1Max);
  SERIAL_API_DEBUG_PRINT("Sensor 0 raw time: %" PRIu32, rawSensor0TimeTakenUs);
  SERIAL_API_DEBUG_PRINT("Sensor 1 raw time: %" PRIu32, rawSensor1TimeTakenUs);
  // SERIAL_API_DEBUG_PRINT("Sensor 0 corrected time: %" PRIu32, (uint32_t)correctedSensor0TimeTakenUs);
  // SERIAL_API_DEBUG_PRINT("Sensor 1 corrected time: %" PRIu32, (uint32_t)correctedSensor1TimeTakenUs);
  SERIAL_API_DEBUG_PRINT("Curtain 1 time: %" PRIu32, curtain1TimeUs);
  SERIAL_API_DEBUG_PRINT("Curtain 2 time: %" PRIu32, curtain2TimeUs);

  CurtainTimings curtainTimings;
  double sensorDistance = 0;
  double frameSize = 0;

  if (curSensorIndex == -1)
  {
    halt("Sensor index is not set");
  }

  SensorUnitData curSensorData = sensorsData[curSensorIndex];
  res.usedSensorType = curSensorData.Type;

  ESP_LOGI("", "Sensor unit name: %s", SensorTypeStr[(uint8_t)curSensorData.Type]);

  if (curtainMovement == CurtainMovement::LEAF)
  {
    res.curtain1spanAtime = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.curtain1spanAspeed = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.curtain1TotalTime = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.curtain2spanAtime = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.curtain2spanAspeed = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.curtain2TotalTime = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.slitWidthSensor0 = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.slitWidthSensor1 = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
    res.slitWidthAverage = NOT_AVAILABLE_FOR_LEAF_SHUTERS;
  }
  else // for focal plane shutters
  {
    if (out.sensor0DataOk && out.sensor1DataOk) // if both sensors data is valid
    {
      curtainTimings.curtain1spanAtime = curtain1TimeUs;
      curtainTimings.curtain2spanAtime = curtain2TimeUs;

      if (curtainMovement == CurtainMovement::HORISONTAL)
      {
        frameSize = curSensorData.FrameWidth;                    // in millimiters
        sensorDistance = curSensorData.HorisontalSensorDistance; // in millimiters
      }
      else if (curtainMovement == CurtainMovement::VERTICAL)
      {
        frameSize = curSensorData.FrameHeight;                 // in millimiters
        sensorDistance = curSensorData.VerticalSensorDistance; // in millimiters
      }

      calculateResults(res, curtainTimings, sensorDistance, frameSize, rawSensor0TimeTakenUs, rawSensor1TimeTakenUs);

      if (res.curtain1spanAspeed < 0 ||
          res.curtain1spanAtime < 0 ||
          res.curtain1TotalTime < 0 ||
          res.curtain2spanAspeed < 0 ||
          res.curtain2spanAtime < 0 ||
          res.curtain2TotalTime < 0 ||
          res.sensor0Time < 0 ||
          res.sensor1Time < 0 ||
          res.slitWidthSensor0 < 0 ||
          res.slitWidthSensor1 < 0 ||
          res.slitWidthAverage < 0)
      { // mark data as invalid
        res.sensor0Time = MEASUREMENTS_IS_INVALID_VAL;
        res.sensor1Time = MEASUREMENTS_IS_INVALID_VAL;
        res.curtain1spanAtime = MEASUREMENTS_IS_INVALID_VAL;
        res.curtain1spanAspeed = MEASUREMENTS_IS_INVALID_VAL;
        res.curtain1TotalTime = MEASUREMENTS_IS_INVALID_VAL;
        res.curtain2spanAtime = MEASUREMENTS_IS_INVALID_VAL;
        res.curtain2spanAspeed = MEASUREMENTS_IS_INVALID_VAL;
        res.curtain2TotalTime = MEASUREMENTS_IS_INVALID_VAL;
        res.slitWidthSensor0 = MEASUREMENTS_IS_INVALID_VAL;
        res.slitWidthSensor1 = MEASUREMENTS_IS_INVALID_VAL;
        res.slitWidthAverage = MEASUREMENTS_IS_INVALID_VAL;
        out.sensor0DataOk = false;
        out.sensor1DataOk = false;
      }
    }
    else
    {
      res.curtain1spanAtime = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.curtain1spanAspeed = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.curtain1TotalTime = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.curtain2spanAtime = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.curtain2spanAspeed = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.curtain2TotalTime = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.slitWidthSensor0 = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.slitWidthSensor1 = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
      res.slitWidthAverage = BOTH_SENSORS_MEASUREMENTS_REQUIRED;
    }
  }
}