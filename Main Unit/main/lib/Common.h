#pragma once

#define TEST_PIN_1 4
#define TEST_PIN_2 5

#define MM_IN_M 1000
#define US_IN_SECOND 1000000
#define US_IN_MINUTE 60000000
#define US_IN_MILLISECOND 1000

#define RESULT_PAGES_COUNT 4

#define SHUTTER_OPEN_LEVEL 1120
#define MIN_ALLOWED_SIGNAL_LEVEL 1300
#define MAX_ALLOWED_SIGNAL_LEVEL 4090
#define MAX_SIGNAL_LEVEL 4096

#define MAIN_MENU_ITEMS_COUNT 4
#define CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT 3
#define SAVE_MEASUREMENT_MENU_ITEMS_COUNT 6
#define SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT 3

#define TOO_SHORT_SENSOR_TIME 0.062 //in milliseconds
#define SENSOR_LIGHT_IS_TOO_DIM -1
#define SENSOR_LIGHT_IS_TOO_BRIGHT -2

#define BOTH_SENSORS_MEASUREMENTS_REQUIRED -4
#define MEASUREMENTS_IS_INVALID_VAL -3
#define NOT_AVAILABLE_FOR_LEAF_SHUTERS -5
#define SENSOR_TIME_IS_TOO_SHORT -6

const uint8_t sensorsDataArraySize = 4;

const char *SensorTypeStr[sensorsDataArraySize] =
{
  "35mm",
  "6x4.5",
  "6x6",
  "6x7"
};

enum class SensorType : uint8_t
{
  Frame35mm,
  Frame6x45,
  Frame6x6,
  Frame6x7,
};

enum class MainMenuItems
{
  MEASURE,
  CHECK_LIGHT,
  MEASURMENT_HISTORY,
  CREDITS,
};

enum class CurtainMovement : uint8_t
{
  HORISONTAL,
  VERTICAL,
  LEAF,
};

enum class MeasurementSaveMenuItems
{
  NO,
  YES,
  OVERWRITE_OLDEST,
  OVERWRITE_NEWEST,
  CHOOSE_RECORD_TO_OVERWRITE,
  BACK_TO_MEASUREMENT_RESULT
};

enum class ZeroRecordsMeasurementSaveMenuItems
{
  NO,
  YES,
  BACK_TO_MEASUREMENT_RESULT
};


CurtainMovement curtainMovement = CurtainMovement::HORISONTAL;
int8_t curSensorIndex = -1;

void formatRecordName(int32_t recordNumber, char resultRecordName[30])
{
  int16_t bufSize = 30;

  #pragma GCC diagnostic ignored "-Wformat"
  if (recordNumber < 10000)
  {
    snprintf(resultRecordName, bufSize, "%04d", recordNumber);
  }
  else if (recordNumber < 100000)
  {
    snprintf(resultRecordName, bufSize, "%05d", recordNumber);
  }
  #pragma GCC diagnostic pop
}

void halt(const char *message)
{
  if (message[0] != '\0')
  {
    // Serial.println(message);
    ESP_LOGE("", "%s", message);
  }

  while (true);
}

void halt()
{
  halt("\0");
}