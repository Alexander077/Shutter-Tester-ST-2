#pragma once

#define RESULT_PAGES_COUNT 4
#define MIN_ALLOWED_SIGNAL_LEVEL 95
#define MAX_ALLOWED_SIGNAL_LEVEL 245
#define MAX_SIGNAL_LEVEL 248

#define MAIN_MENU_ITEMS_COUNT 4
#define CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT 3
#define SAVE_MEASUREMENT_MENU_ITEMS_COUNT 5

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