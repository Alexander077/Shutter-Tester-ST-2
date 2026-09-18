#pragma once

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "AlexButton.h"

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

// User-editable record name limits. Stored as fixed char
// array in StoredMeasuredResult / StoredMeasuredSeries, always '\0'-terminated.
#define RECORD_NAME_MAX_LEN 40
#define RECORD_NAME_BUF_SIZE (RECORD_NAME_MAX_LEN + 1)

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

#define ENCODER_B_PIN 13 // s1
#define ENCODER_A_PIN 27 // s2
#define BUTTON_PIN 14

AlexButton button(BUTTON_PIN);

// NOTE: ISR callbacks must NOT be 'inline' on ESP32 (COMDAT section placement
// breaks IRAM literal relocation), so this one is 'static' instead.
static void IRAM_ATTR onTimer()
{
  button.tick();
  // AlexEncoder::tick();
}

#define USER_INPUT_POLLING_FREQ_HZ 300
// #define SPLASH_SCREEN_VISIBLE_TIME_MS 1000000000
#define SPLASH_SCREEN_VISIBLE_TIME_MS 1250

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

// Number of visible characters in a record name (trailing spaces trimmed,
// like the record list shows it). Embedded '\0' ends the scan.
uint8_t getRecordNameVisibleLen(const char *name)
{
  uint8_t len = 0;

  if (name == nullptr)
  {
    return 0;
  }

  for (uint8_t i = 0; i < RECORD_NAME_MAX_LEN && name[i] != '\0'; i++)
  {
    len = i + 1;
  }

  while (len > 0 && name[len - 1] == ' ')
  {
    len--;
  }

  return len;
}

// True when the name holds no visible characters (empty or spaces only).
bool isRecordNameEmpty(const char *name)
{
  return getRecordNameVisibleLen(name) == 0;
}

// Copies src into dst (RECORD_NAME_BUF_SIZE), always '\0'-terminated.
void copyRecordName(char *dst, const char *src)
{
  if (dst == nullptr)
  {
    return;
  }

  if (src == nullptr)
  {
    dst[0] = '\0';
    return;
  }

  strncpy(dst, src, RECORD_NAME_MAX_LEN);
  dst[RECORD_NAME_MAX_LEN] = '\0';
}

// Fills dst with the legacy "0002"-style name for the given record number.
void formatDefaultRecordName(int32_t recordNumber, char *dst)
{
  char buf[30];
  formatRecordName(recordNumber, buf);
  copyRecordName(dst, buf);
}

static char recordNameToLowerChar(char c)
{
  if (c >= 'A' && c <= 'Z')
  {
    return (char)(c + ('a' - 'A'));
  }

  return c;
}

// Case-insensitive prefix check (answers Q5: "не кейс сенситив").
bool recordNameStartsWithIgnoreCase(const char *str, const char *prefix, uint8_t prefixLen)
{
  if (str == nullptr || prefix == nullptr)
  {
    return false;
  }

  for (uint8_t i = 0; i < prefixLen; i++)
  {
    if (str[i] == '\0')
    {
      return false;
    }

    if (recordNameToLowerChar(str[i]) != recordNameToLowerChar(prefix[i]))
    {
      return false;
    }
  }

  return true;
}

// Case-insensitive equality of visible (trailing-trimmed) names.
bool recordNamesEqualIgnoreCase(const char *a, const char *b)
{
  uint8_t aLen = getRecordNameVisibleLen(a);
  uint8_t bLen = getRecordNameVisibleLen(b);

  if (aLen != bLen)
  {
    return false;
  }

  if (a == nullptr || b == nullptr)
  {
    return false;
  }

  for (uint8_t i = 0; i < aLen; i++)
  {
    if (recordNameToLowerChar(a[i]) != recordNameToLowerChar(b[i]))
    {
      return false;
    }
  }

  return true;
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