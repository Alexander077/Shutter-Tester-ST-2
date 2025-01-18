#pragma once

#define RESULT_PAGES_COUNT 4

#define SHUTTER_OPEN_LEVEL 50
#define MIN_ALLOWED_SIGNAL_LEVEL SHUTTER_OPEN_LEVEL
#define MAX_ALLOWED_SIGNAL_LEVEL 230
#define MAX_SIGNAL_LEVEL 249

#define MAIN_MENU_ITEMS_COUNT 4
#define CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT 3
#define SAVE_MEASUREMENT_MENU_ITEMS_COUNT 5

#define START_LIGHT_TEMP_C 20
#define MAX_LIGHT_TEMP_C 35
#define NO_TEMP_READING -10000
#define DISCONNECTED_LIGHT_TEMP_VALUE -127

enum class MainMenuItems
{
  MEASURE,
  CHECK_LIGHT,
  MEASURMENT_HISTORY,
  CREDITS,
};

enum class CurtainMovement
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
};

enum class Screens
{
  MAIN_MENU = 'm',
  MEASURING = 'n',
  MEASURED = 'r',
  CREDITS = 'c',
  CURTAN_MOVEMENT_SELECTION = 'd',
  LIGHT_CHECK = 'l',
  MEASUREMENT_SAVE_SCREEN = 's',
  MEASUREMENT_RECORD_SELECTION = 'h',
  MESSAGE = 'g',
  TEST = 't',
};

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
    Serial.println(message);
  }

  while (true);
}

void halt(const __FlashStringHelper *message)
{
  int16_t bufferSize = strlen_P((const char *)message) + 1;
  char buffer[bufferSize];
  strncpy_P(buffer, (const char *)message, bufferSize);
  halt(buffer);
}

void halt()
{
  halt("\0");
}