
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include "Arduino.h"
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <mString.h>
#include "lib/InterpolationLib/src/InterpolationLib.h"
#include "lib/AceSorting/src/AceSorting.h"

#include "lib/AlexEncoder.h"
#include "lib/AlexButton.h"
#include "lib/Common.h"
#include "lib/DisplayManager.h"
#include "lib/StoredMeasuredResult.h"
#include "lib/About.h"

#define DISPLAY_I2C_ADDRESS 4
// #define DISPLAY_CS SS
#define DISPLAY_RESET 18
#define DISPLAY_DC 33

#define ENCODER_B_PIN 12
#define ENCODER_A_PIN 13

#define BUTTON_PIN 14
#define TEST_PIN 5

#define USER_INPUT_POLLING_FREQ_HZ 100
// #define SPLASH_SCREEN_VISIBLE_TIME_MS 1000000000
#define SPLASH_SCREEN_VISIBLE_TIME_MS 1250

#define MM_IN_M 1000
#define US_IN_SECOND 1000000
#define US_IN_MILLISECOND 1000

#define SENSOR_TYPE_CODE_PIN A7

// EEPROM memory layout: |saved measures (0-1012)|first run value(1023)|
#define EEPROM_FIRST_RUN_VAL_INDEX 1023
#define EEPROM_FIRST_RUN_VAL 123

#define EEPROM_MEASURED_RES_START_INDEX 0
#define EEPROM_MEASURED_RES_END_INDEX 1012
#define EEPROM_MEASURED_RES_TOTAL_BYTES (EEPROM_MEASURED_RES_END_INDEX - EEPROM_MEASURED_RES_START_INDEX)

static const char *TAG = "";

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
// Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_DataBus *bus = new Arduino_HWSPI(DISPLAY_DC, SS, SCK, MOSI);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *display = new Arduino_ST7735(
    bus, DISPLAY_RESET, 1 /* rotation */, false /* IPS */,
    128 /* width */, 160 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */,
    0 /* col offset 2 */, 0 /* row offset 2 */,
    false /* BGR */);

AlexButton button(BUTTON_PIN);

enum class AdcISRFlow
{
  NONE,
  MEASURING,
  // MEASURED,
  SENSOR_READINGS_CHECK,
  // PWM_LIGHT_CHECK,
  // FAST_MEASURING,
  // FAST_MEASURED
};

struct CurtainTimings
{
  double curtain1spanAtime; // In microseconds
  double curtain2spanAtime; // In microseconds
};

struct MeasurementRecordSaveResult
{
  bool isSuccess;
  int32_t newRecordNumber;
};

enum class MeasurementSaveScreenResult
{
  OK,
  CANCEL
};

volatile long pin0shutterOpenStartTime = -1,
              pin0shutterOpenEndTime = -1,
              pin1shutterOpenStartTime = -1,
              pin1shutterOpenEndTime = -1;

AdcISRFlow adcISRFlow = AdcISRFlow::NONE;
DisplayManager displayManager;

bool curADCpinNumber = true;
volatile uint8_t sensor0SignalLevel = 0;
volatile uint8_t sensor1SignalLevel = 0;
volatile uint16_t sensor0BlinkCount = 0;
volatile uint16_t sensor1BlinkCount = 0;

// 3 times per second
#define SENSOR_VALUES_UPDATE_INTERVAL 333
#define SENSOR_MAX_AVG_ARRAY_SIZE 5
#define SENSOR_MAX_BLINK_COUNT_PER_INTERVAL 4
uint8_t sensor0Max = 0; // volatile?
uint8_t sensor1Max = 0; // volatile?
uint8_t sensor0MaxArr[SENSOR_MAX_AVG_ARRAY_SIZE] = {};
uint8_t sensor1MaxArr[SENSOR_MAX_AVG_ARRAY_SIZE] = {};

uint16_t sensorCheckCounter = 0;
#define SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE 1000

#define INTRPOLATION_POINTS_COUNT 4
double sensorMaxAdcVals[INTRPOLATION_POINTS_COUNT] = {80, 110, 140, 230};
double timeCorrectionVals[INTRPOLATION_POINTS_COUNT] = {15, 25, 30, 0};

#define SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX 5
#define SPEEDS_GRAPH_BAR_WIDTH_PX 146

#define SHUTTR_SPEEDS_COUNT 14
const uint16_t shutterSpeeds[] = {8000, 4000, 2000, 1000, 500, 250, 125, 60, 30, 15, 8, 4, 2, 1};

CurtainMovement curtainMovement = CurtainMovement::HORISONTAL;

struct SensorUnitData
{
  SensorType Type;
  double FrameWidth;               // in millimiters
  double FrameHeight;              // in millimiters
  double HorisontalSensorDistance; // in millimiters
  double VerticalSensorDistance;   // in millimiters
  uint16_t minAdcVal;
  uint16_t maxAdcVal;
};

const SensorUnitData sensorsData[sensorsDataArraySize] = {
    {SensorType::Frame35mm,
     36,
     24,
     16.0,
     10.6,
     700, // 3.3 kOhm
     840},
    {SensorType::Frame6x45,
     60,
     45,
     26.67,
     20.0,
     160, // 47 kOhm
     196},
    {SensorType::Frame6x6,
     60,
     60,
     26.67,
     26.67,
     900, // 1 kOhm
     940},
    {SensorType::Frame6x7,
     70,
     60,
     31.11,
     26.67,
     82, // 100 kOhm
     100}};

struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

const char *MainMenuItemsStr[MAIN_MENU_ITEMS_COUNT] =
    {
        "Measure",
        "Light Setup",
        "Saved Measures",
        "About"};

const char *CurtainMovementItemsStr[CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT] =
    {
        "Horisontal",
        "Vertical",
        "Leaf"};

const char *MeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ITEMS_COUNT] =
    {
        "No",
        "Yes",
        "Overwrite oldest",
        "Overwrite newest",
        "Sel. rec. to overwrite",
        "Return to result",
};

const char *ZeroRecordsMeasurementSaveItemsStr[SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT] =
    {
        "No",
        "Yes",
        "Return to result",
};

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

const char *GetCurtainMovementItemTitle(CurtainMovement menuItem)
{
  return CurtainMovementItemsStr[(uint8_t)menuItem];
}

const char *GetSaveMasurementMenuItemTitle(MeasurementSaveMenuItems menuItem, bool useForZeroRecords)
{
  if (useForZeroRecords)
  {
    return ZeroRecordsMeasurementSaveItemsStr[(uint8_t)menuItem];
  }
  else
  {
    return MeasurementSaveItemsStr[(uint8_t)menuItem];
  }
}

Rect getStringRect(const char *str, int16_t x, int16_t y)
{
  // Variables to hold the bounding box dimensions
  int16_t x1, y1;
  uint16_t w, h;

  // Get the bounds of the text
  display->getTextBounds(str, x, y, &x1, &y1, &w, &h);

  Rect r;
  r.x = x1;
  r.y = y1;
  r.width = w;
  r.height = h;
  return r;
}

Rect drawStringHCentered(const char *str, uint16_t y)
{
  // // Variables to hold the bounding box dimensions
  // int16_t x1, y1;
  // uint16_t w, h;

  Rect r = getStringRect(str, 0, y);
  // Serial.println(r.y);

  // Calculate the position to center the text on the screen
  int16_t x = (display->width() - r.width) / 2;
  // int16_t y = (display->height() - h) / 2;
  r.x = x;

  // Draw the text at the calculated position
  display->setCursor(x, y);
  display->print(str);

  // Serial.print(r.x);
  // Serial.print("|");
  // Serial.print(r.y);
  // Serial.print("|");
  // Serial.print(r.width);
  // Serial.print("|");
  // Serial.println(r.height);

  // Optional: Draw the bounding box around the text for visualization
  // display->drawRect(x, y - r.height, r.width, r.height, RED);
  return r;
}

Rect drawStringHCentered(const String &str, uint16_t y)
{
#define CHAR_BUF_SIZE 50
  char buf[CHAR_BUF_SIZE];
  str.toCharArray(buf, CHAR_BUF_SIZE);
  return drawStringHCentered(buf, y);
}

void drawNavBar(int16_t activePageIndex, int16_t itemsCount)
{
  int16_t navBarWidth = 120;
  int16_t navBarY = 120;
  // int16_t navBarItemsCount = RESULT_PAGES_COUNT;
  int16_t navBarLeftMargin = (display->width() - navBarWidth) / 2;
  display->drawLine(navBarLeftMargin, navBarY, navBarLeftMargin + navBarWidth, navBarY, WHITE);

  for (int8_t i = 0, spacing = 0; i < itemsCount; i++, spacing += navBarWidth / (itemsCount - 1))
  {
    display->fillCircle(navBarLeftMargin + spacing, navBarY, i == activePageIndex ? 4 : 2, i == activePageIndex ? RGB565(0, 102, 153) : WHITE);
  }
}

void IRAM_ATTR onTimer()
{
  button.tick();
}

/* bool isFirstRun()
{
  return EEPROM.read(EEPROM_FIRST_RUN_VAL_INDEX) == EEPROM_FIRST_RUN_VAL;
} */

int16_t getMeasurementSaveRecordSize()
{
  return sizeof(StoredMeasuredResult);
}

int16_t getTotalMeasurementSaveSlotsCount()
{
  return EEPROM_MEASURED_RES_TOTAL_BYTES / getMeasurementSaveRecordSize();
}

/* int16_t getFreeMeasurementSaveSlotsCount()
{
  int16_t totalCount = getTotalMeasurementSaveSlotsCount();
  int16_t blockSize = getMeasurementSaveRecordSize();
  int16_t freeCount = 0;
  StoredMeasuredResult tempMeasRes;

  for (int16_t i = 0; i < totalCount; i++)
  {
    EEPROM.get(i * blockSize, tempMeasRes);

    if (tempMeasRes.recordNumber == 0 || tempMeasRes.isDeleted)
    {
      freeCount++;
    }
  }
  return freeCount;
} */

/* StoredMeasuredResult getMeasurementStoredResultByNumber(int32_t recordNumber)
{
  int16_t totalCount = getTotalMeasurementSaveSlotsCount();
  int16_t blockSize = getMeasurementSaveRecordSize();
  StoredMeasuredResult measRes = {-1};

  for (int16_t i = 0; i < totalCount; i++)
  {
    EEPROM.get(i * blockSize, measRes);

    if (measRes.recordNumber == recordNumber)
    {
      return measRes;
    }
  }
  return measRes;
} */

/* bool deleteMeasurementStoredResultByNumber(int32_t recordNumber)
{
  int16_t totalCount = getTotalMeasurementSaveSlotsCount();
  int16_t blockSize = getMeasurementSaveRecordSize();
  StoredMeasuredResult measRes = {-1};

  for (int16_t i = 0; i < totalCount; i++)
  {
    EEPROM.get(i * blockSize, measRes);

    if (measRes.recordNumber == recordNumber)
    {
      measRes.isDeleted = true;
      return verifiedEEPROMPut(i * blockSize, measRes);
    }
  }
} */

/* int32_t drawMeasurementResultRecordSelectionScreen()
{
  StoredMeasuredResult tempMeasRes;
  int16_t blockSize = getMeasurementSaveRecordSize();
  int16_t totalSlotsCount = getTotalMeasurementSaveSlotsCount();
  int16_t freeRecordSlots = getFreeMeasurementSaveSlotsCount();
  int16_t existingRecordsCount = totalSlotsCount - freeRecordSlots;
  int16_t totalArraySaize = existingRecordsCount + 1;
  int32_t recordNumbers[totalArraySaize];
  recordNumbers[0] = 0; //"Back" option
  // char *recordTitles[totalArraySaize];

  for (int16_t i = 0, r = 1; i < totalSlotsCount; i++)
  {
    EEPROM.get(i * blockSize, tempMeasRes);

    if (tempMeasRes.recordNumber > 0 && !tempMeasRes.isDeleted)
    {

      recordNumbers[r] = tempMeasRes.recordNumber;
      r++;
    }
  }

  // recordNumbers[0] = 0;
  // recordNumbers[1] = 89345;
  // recordNumbers[2] = 456;
  // recordNumbers[3] = 321;
  // recordNumbers[4] = 5776;
  // recordNumbers[5] = 57623;
  // recordNumbers[6] = 120381;
  // recordNumbers[7] = 335;
  // recordNumbers[8] = 5675;
  // recordNumbers[9] = 768;
  // recordNumbers[10] = 57;
  // recordNumbers[11] = 789;

  ace_sorting::shellSortKnuth(recordNumbers, totalArraySaize);

  int16_t startEncoderVal = AlexEncoder::counter;

  while (true)
  {
    int16_t curIndex = AlexEncoder::counter - startEncoderVal;

    if (curIndex > totalArraySaize - 1)
    {
      startEncoderVal = AlexEncoder::counter - (totalArraySaize - 1);
      curIndex = totalArraySaize - 1;
    }

    if (curIndex < 0)
    {
      curIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    displayManager.drawMeasurementResultRecordSelectionScreen(curIndex, recordNumbers, totalArraySaize);

    if (button.isClicked())
    {
      return recordNumbers[curIndex];
    }
  }
} */

/* MeasurementRecordSaveResult saveMesurementResult(const MeasuredResult &res, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
{
  int32_t numberOfLast = -1;
  int32_t indexOfLast = -1;
  StoredMeasuredResult savedMeasRes;
  StoredMeasuredResult tempMeasRes;
  int16_t blockSize = getMeasurementSaveRecordSize();
  int16_t totalRecordsCount = getTotalMeasurementSaveSlotsCount();
  MeasurementRecordSaveResult saveRes = {false, -1};

  for (int16_t i = 0; i < totalRecordsCount; i++)
  {
    EEPROM.get(i * blockSize, tempMeasRes);

    if (tempMeasRes.recordNumber > numberOfLast && !tempMeasRes.isDeleted)
    {
      numberOfLast = tempMeasRes.recordNumber;
      indexOfLast = i;
    }
  }

  if (numberOfLast == -1)
  {
    halt(F("Halted. numberOfLast was not defined"));
  }

  savedMeasRes.recordNumber = numberOfLast + 1;
  savedMeasRes.isDeleted = false;
  savedMeasRes.sensor0Time = res.sensor0Time;
  savedMeasRes.sensor1Time = res.sensor1Time;
  savedMeasRes.curtain1spanAspeed = res.curtain1spanAspeed;
  savedMeasRes.curtain1spanAtime = res.curtain1spanAtime;
  savedMeasRes.curtain1TotalTime = res.curtain1TotalTime;
  savedMeasRes.curtain2spanAspeed = res.curtain2spanAspeed;
  savedMeasRes.curtain2spanAtime = res.curtain2spanAtime;
  savedMeasRes.curtain2TotalTime = res.curtain2TotalTime;
  savedMeasRes.slitWidthSensor0 = res.slitWidthSensor0;
  savedMeasRes.slitWidthSensor1 = res.slitWidthSensor1;
  savedMeasRes.slitWidthAverage = res.slitWidthAverage;
  savedMeasRes.usedSensorType = res.usedSensorType;
  savedMeasRes.selectedCurtainMovement = res.selectedCurtainMovement;

  if (numberOfLast == 0) // if there is no records at all in the EEPROM
  {
    saveRes.isSuccess = verifiedEEPROMPut(EEPROM_MEASURED_RES_START_INDEX, savedMeasRes);

    if (saveRes.isSuccess)
    {
      saveRes.newRecordNumber = savedMeasRes.recordNumber;
    }

    Serial.print("Meas. res. savaed. Rec. num.: ");
    Serial.print(savedMeasRes.recordNumber);
    Serial.print(", success: ");
    Serial.println(saveRes.isSuccess);
    return saveRes;
  }

  int16_t freeSlotIndex = -1;

  // save to free slot
  if (!overwriteNewest && !overwriteOldest && recordNumberToOverwrite == -1)
  {
    // Check for free slots
    for (int16_t i = 0; i < totalRecordsCount; i++)
    {
      EEPROM.get(i * blockSize, tempMeasRes);

      if (tempMeasRes.recordNumber == 0 || tempMeasRes.isDeleted)
      {
        freeSlotIndex = i;
      }
    }

    if (freeSlotIndex != -1)
    {
      saveRes.isSuccess = verifiedEEPROMPut(freeSlotIndex * blockSize, savedMeasRes);
      if (saveRes.isSuccess)
      {
        saveRes.newRecordNumber = savedMeasRes.recordNumber;
      }
      Serial.print("Meas. res. savaed. Rec. num.: ");
      Serial.print(savedMeasRes.recordNumber);
      Serial.print(", success: ");
      Serial.println(saveRes.isSuccess);
      return saveRes;
    }
    halt(F("Halted. freeSlotIndex not found"));
  }

  // if user choose to overwrite oldest record
  if (overwriteOldest)
  {
    int32_t numberOfOldest = INT32_MAX;
    int16_t indexOfOldest = -1;

    // find oldest
    for (int16_t i = 0; i < totalRecordsCount; i++)
    {
      EEPROM.get(i * blockSize, tempMeasRes);

      if (tempMeasRes.recordNumber != 0 && tempMeasRes.recordNumber < numberOfOldest && !tempMeasRes.isDeleted)
      {
        numberOfOldest = tempMeasRes.recordNumber;
        indexOfOldest = i;
      }
    }

    if (indexOfOldest == -1)
    {
      halt(F("Halted. indexOfOldest not found"));
    }

    saveRes.isSuccess = verifiedEEPROMPut(indexOfOldest * blockSize, savedMeasRes);

    if (saveRes.isSuccess)
    {
      saveRes.newRecordNumber = savedMeasRes.recordNumber;
    }

    Serial.print("Meas. res. savaed. Rec. num.: ");
    Serial.print(savedMeasRes.recordNumber);
    Serial.print(", success: ");
    Serial.println(saveRes.isSuccess);
    return saveRes;
  }

  // if user choose to overwrite newest record
  if (overwriteNewest)
  {
    if (indexOfLast == -1)
    {
      halt(F("Halted. indexOfLast not found"));
    }

    saveRes.isSuccess = verifiedEEPROMPut(indexOfLast * blockSize, savedMeasRes);

    if (saveRes.isSuccess)
    {
      saveRes.newRecordNumber = savedMeasRes.recordNumber;
    }

    Serial.print("Meas. res. savaed. Rec. num.: ");
    Serial.print(savedMeasRes.recordNumber);
    Serial.print(", success: ");
    Serial.println(saveRes.isSuccess);
    return saveRes;
  }

  if (recordNumberToOverwrite != -1)
  {
    for (int16_t i = 0; i < totalRecordsCount; i++)
    {
      EEPROM.get(i * blockSize, tempMeasRes);

      if (tempMeasRes.recordNumber == recordNumberToOverwrite)
      {
        saveRes.isSuccess = verifiedEEPROMPut(i * blockSize, savedMeasRes);

        if (saveRes.isSuccess)
        {
          saveRes.newRecordNumber = savedMeasRes.recordNumber;
        }

        Serial.print("Meas. res. savaed. Rec. num.: ");
        Serial.print(savedMeasRes.recordNumber);
        Serial.print(", success: ");
        Serial.println(saveRes.isSuccess);
        return saveRes;
      }
    }

    halt(F("Halted. recordNumberToOverwrite not found"));
  }

  halt(F("Halted. Program should not get here"));
} */

double getCorrectedSensorValue(long rawSensorTime, uint8_t maxSensorValue)
{
  // #pragma GCC diagnostic ignored "-fpermissive"
  double correction = Interpolation::Linear(sensorMaxAdcVals, timeCorrectionVals, INTRPOLATION_POINTS_COUNT, (double)maxSensorValue, false);
  // #pragma GCC diagnostic pop
  double resSensorTime = rawSensorTime + correction;
  return resSensorTime;
}

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

int16_t _drawMessageScreen(bool useProgMem, const char *title, int16_t optionsCount = 0, const char *options[] = {})
{
  int16_t startEncoderVal = AlexEncoder::counter;
  int32_t messageScreenId = random(INT32_MAX);

  while (true)
  {
    int16_t resultOptionIndex = AlexEncoder::counter - startEncoderVal;

    if (resultOptionIndex > optionsCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (optionsCount - 1);
      resultOptionIndex = optionsCount - 1;
    }

    if (resultOptionIndex < 0)
    {
      resultOptionIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    displayManager.drawMessageScreen(messageScreenId, useProgMem, title, options, optionsCount, resultOptionIndex);

    if (button.isClicked())
    {
      return resultOptionIndex;
    }
  }
}

int16_t drawMessageScreen_P(const char *title, int16_t optionsCount = 0, const char *options[] = {})
{
  return _drawMessageScreen(true, title, optionsCount, options);
}

int16_t drawMessageScreen(const char *title, int16_t optionsCount = 0, const char *options[] = {})
{
  return _drawMessageScreen(false, title, optionsCount, options);
}

/* MeasurementSaveScreenResult drawMeasurementSaveScreen(MeasuredResult &measurementRes)
{
  int16_t freeSlotsCount = getFreeMeasurementSaveSlotsCount();
  int16_t totalRecordsCount = getTotalMeasurementSaveSlotsCount();
  int16_t menuItemsCount = totalRecordsCount == freeSlotsCount ? SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT : SAVE_MEASUREMENT_MENU_ITEMS_COUNT;
  int16_t startEncoderVal = AlexEncoder::counter;
  bool drawMoreThanZeroRecordsMenuItems = menuItemsCount == SAVE_MEASUREMENT_MENU_ITEMS_COUNT;

  while (true)
  {
    int16_t resultIndex = AlexEncoder::counter - startEncoderVal;

    if (resultIndex > menuItemsCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (menuItemsCount - 1);
      resultIndex = menuItemsCount - 1;
    }

    if (resultIndex < 0)
    {
      resultIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    displayManager.drawMeasurementSaveScreen(resultIndex, freeSlotsCount, drawMoreThanZeroRecordsMenuItems);

    if (button.isClicked())
    {
      MeasurementRecordSaveResult saveRes;

      if (drawMoreThanZeroRecordsMenuItems)
      {
        switch ((MeasurementSaveMenuItems)resultIndex)
        {
        case MeasurementSaveMenuItems::NO:
        {
          return MeasurementSaveScreenResult::OK; // do not save the result
          break;
        }
        case MeasurementSaveMenuItems::YES:
        {
          if (freeSlotsCount > 0)
          {
            Serial.println("Saving record...");
            saveRes = saveMesurementResult(measurementRes);
          }
          else
          {
            continue;
          }

          break;
        }
        case MeasurementSaveMenuItems::OVERWRITE_NEWEST:
        {
          Serial.println("Saving record...");
          saveRes = saveMesurementResult(measurementRes, false, true);
          break;
        }
        case MeasurementSaveMenuItems::OVERWRITE_OLDEST:
        {
          Serial.println("Saving record...");
          saveRes = saveMesurementResult(measurementRes, true);
          break;
        }
        case MeasurementSaveMenuItems::CHOOSE_RECORD_TO_OVERWRITE:
        {
          int32_t selectedMesurementRecordNumber = drawMeasurementResultRecordSelectionScreen();

          if (selectedMesurementRecordNumber == 0) // user selected "Back" option
          {
            continue;
          }

          Serial.println("Saving record...");
          saveRes = saveMesurementResult(measurementRes, false, false, selectedMesurementRecordNumber);
          break;
        }
        case MeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
        {
          return MeasurementSaveScreenResult::CANCEL;
          break;
        }
        default:
          halt(F("Halted. Unknown measurement save option"));
          break;
        }
      }
      else
      {
        switch ((ZeroRecordsMeasurementSaveMenuItems)resultIndex)
        {
        case ZeroRecordsMeasurementSaveMenuItems::NO:
        {
          return MeasurementSaveScreenResult::OK; // do not save the result
          break;
        }
        case ZeroRecordsMeasurementSaveMenuItems::YES:
        {
          if (freeSlotsCount > 0)
          {
            Serial.println("Saving record...");
            saveRes = saveMesurementResult(measurementRes);
          }
          else
          {
            continue;
          }

          break;
        }
        case ZeroRecordsMeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
        {
          return MeasurementSaveScreenResult::CANCEL;
          break;
        }
        default:
          halt(F("Halted. Unknown measurement save option"));
          break;
        }
      }

      char resMessage[50];

      if (saveRes.isSuccess)
      {
        char buf[30];
        formatRecordName(saveRes.newRecordNumber, buf);
#pragma GCC diagnostic ignored "-Wformat"
        sprintf(resMessage, "Record saved as '%s'", buf);
#pragma GCC diagnostic pop
      }
      else
      {
        sprintf(resMessage, "Failed to save record");
      }
      const char *option[] = {"OK"};
      drawMessageScreen(resMessage, 1, option);
      return MeasurementSaveScreenResult::OK;
    }
  }
} */

/* void drawMeasuredScreen()
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


    // Sensor 0 timecodes: 16599544|16601460
    // Sensor 1 timecodes: 16590476|16592600
    // Sensor 0 max val.: 163
    // Sensor 1 max val.: 167

    // pin0shutterOpenStartTime = 16599544;
    // pin0shutterOpenEndTime = 16601460;
    // pin1shutterOpenStartTime = -1;
    // pin1shutterOpenEndTime = -1;
    // sensor0Max = 163;
    // sensor1Max = 20;
    // curtainMovement = CurtainMovement::HORISONTAL;

    // Serial.println(String("Sensor 0 timecodes: ") + pin0shutterOpenStartTime + "|" + pin0shutterOpenEndTime);
    // Serial.println(String("Sensor 1 timecodes: ") + pin1shutterOpenStartTime + "|" + pin1shutterOpenEndTime);
    // Serial.println(String("Sensor 0 max val.: ") + sensor0Max);
    // Serial.println(String("Sensor 1 max val.: ") + sensor1Max);

    bool sensor0DataOk = false;
    bool sensor1DataOk = false;
    double rawSensor0TimeTakenUs = -1;
    double rawSensor1TimeTakenUs = -1;
    double correctedSensor0TimeTakenUs = -1;
    double correctedSensor1TimeTakenUs = -1;

    // Serial.println(String("Sensor 0 time taken: ") + (sensor0CorrectedTime / 1000.0) + " ms");
    // Serial.println(String("Sensor 1 time taken: ") + (sensor1CorrectedTime / 1000.0) + " ms");

    MeasuredResult res;
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
    rawSensor0TimeTakenUs = pin0shutterOpenEndTime - pin0shutterOpenStartTime;                // in microseconds
    correctedSensor0TimeTakenUs = getCorrectedSensorValue(rawSensor0TimeTakenUs, sensor0Max); // in microseconds
    res.sensor0Time = correctedSensor0TimeTakenUs / US_IN_MILLISECOND;                        // in milliseconds

    if (res.sensor0Time < TOO_SHORT_SENSOR_TIME)
    {
      res.sensor0Time = SENSOR_TIME_IS_TOO_SHORT;
    }
    else
    {
      sensor0DataOk = true;
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
    rawSensor1TimeTakenUs = pin1shutterOpenEndTime - pin1shutterOpenStartTime;                // in microseconds
    correctedSensor1TimeTakenUs = getCorrectedSensorValue(rawSensor1TimeTakenUs, sensor1Max); // in microseconds
    res.sensor1Time = correctedSensor1TimeTakenUs / US_IN_MILLISECOND;                        // in milliseconds

    if (res.sensor1Time < TOO_SHORT_SENSOR_TIME)
    {
      res.sensor1Time = SENSOR_TIME_IS_TOO_SHORT;
    }
    else
    {
      sensor1DataOk = true;
    }
  }

  Serial.print("Sensor 0 max: ");
  Serial.println(sensor0Max);

  Serial.print("Sensor 1 max: ");
  Serial.println(sensor1Max);

  CurtainTimings curtainTimings;
  double sensorDistance;
  double frameSize;

  // setADCprescaler(ADCPrescaler::ADC_PRESCALER_128); // needed to correctly read the sensor code
  delay(100);
  uint16_t curSensorCode = analogRead(SENSOR_TYPE_CODE_PIN);
  // uint16_t curSensorCode = 927;//TODO: mocked, comment or remove
  // setupADC(); // resetup ADC
  Serial.print("Sensor unit code: ");
  Serial.println(curSensorCode);

  int8_t curSensorDataIndex = -1;

  for (size_t i = 0; i < sensorsDataArraySize; i++)
  {
    if (curSensorCode >= sensorsData[i].minAdcVal && curSensorCode <= sensorsData[i].maxAdcVal)
    {
      curSensorDataIndex = i;
      break;
    }
  }

  if (curSensorDataIndex == -1)
  {
    halt("Sensor data not found. Program halted");
  }

  SensorUnitData curSensorData = sensorsData[curSensorDataIndex];
  res.usedSensorType = curSensorData.Type;

  Serial.print("Sensor unit name: ");
  Serial.println(SensorTypeStr[(uint8_t)curSensorData.Type]);

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
    if (sensor0DataOk && sensor1DataOk) // if both sensors data is valid
    {
      if (curtainMovement == CurtainMovement::HORISONTAL || curtainMovement == CurtainMovement::VERTICAL)
      {
        if (pin0shutterOpenStartTime < pin1shutterOpenStartTime) // left to right or top to bottom curtans movement
        {
          curtainTimings.curtain1spanAtime = (double)(pin1shutterOpenStartTime - pin0shutterOpenStartTime);
          curtainTimings.curtain2spanAtime = (double)(pin1shutterOpenEndTime - pin0shutterOpenEndTime);
        }
        else // right to left or bottom to top curtans movement
        {
          curtainTimings.curtain1spanAtime = (double)(pin0shutterOpenStartTime - pin1shutterOpenStartTime);
          curtainTimings.curtain2spanAtime = (double)(pin0shutterOpenEndTime - pin1shutterOpenEndTime);
        }

        if (curtainMovement == CurtainMovement::HORISONTAL)
        {
          frameSize = curSensorData.FrameWidth;                    // in millimiters
          sensorDistance = curSensorData.HorisontalSensorDistance; // in millimiters
        }
        else
        {
          frameSize = curSensorData.FrameHeight;                 // in millimiters
          sensorDistance = curSensorData.VerticalSensorDistance; // in millimiters
        }
      }

      calculateResults(res, curtainTimings, sensorDistance, frameSize, correctedSensor0TimeTakenUs, correctedSensor1TimeTakenUs);

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
        sensor0DataOk = false;
        sensor1DataOk = false;
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

  // Serial.println(res.curtain1spanAspeed);
  // Serial.println(res.curtain1spanAtime);
  // Serial.println(res.curtain1TotalTime);
  // Serial.println(res.curtain2spanAspeed);
  // Serial.println(res.curtain2spanAtime);
  // Serial.println(res.curtain2TotalTime);
  // Serial.println(res.sensor0Time);
  // Serial.println(res.sensor1Time);
  // Serial.println(res.slitWidthSensor0);
  // Serial.println(res.slitWidthSensor1);
  // Serial.println(res.slitWidthAverage);

  int16_t startEncoderVal = AlexEncoder::counter;

  while (true)
  {
    int16_t resultPageIndex = AlexEncoder::counter - startEncoderVal;

    if (resultPageIndex > RESULT_PAGES_COUNT - 1)
    {
      startEncoderVal = AlexEncoder::counter - (RESULT_PAGES_COUNT - 1);
      resultPageIndex = RESULT_PAGES_COUNT - 1;
    }

    if (resultPageIndex < 0)
    {
      resultPageIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    displayManager.drawMeasuredScreen(resultPageIndex, res);

    if (button.isClicked())
    {
      if (sensor0DataOk || sensor1DataOk)
      {
        auto screenRes = drawMeasurementSaveScreen(res);

        if (screenRes == MeasurementSaveScreenResult::CANCEL)
        {
          startEncoderVal = AlexEncoder::counter;
          continue;
        }
      }

      return;
    }
  }
} */

/* void drawMeasuringScreen()
{
  pin0shutterOpenStartTime = -1;
  pin0shutterOpenEndTime = -1;
  pin1shutterOpenStartTime = -1;
  pin1shutterOpenEndTime = -1;
  sensor0Max = 0;
  sensor1Max = 0;

  adcISRFlow = AdcISRFlow::MEASURING;
  setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
  startADCconversion(); // start first ADC conversion

  while (true)
  {
    displayManager.drawMeasuringScreen();

    if (button.isClicked())
    {
      adcISRFlow = AdcISRFlow::NONE;
      return;
    }
    else if (adcISRFlow == AdcISRFlow::MEASURING &&
             ((pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime != -1) ||
              (pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime != -1))) // Check if at least one sensor has data
    {
      delay(500); // wait for other sensors to get data
      adcISRFlow = AdcISRFlow::NONE;
      drawMeasuredScreen();
      return;
    }
  }
} */

/* void drawCurtainMovementSelectionScreen()
{
  int16_t startEncoderVal = AlexEncoder::counter;

  while (true)
  {
    int16_t resultMenuItemIndex = AlexEncoder::counter - startEncoderVal;

    if (resultMenuItemIndex > CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT - 1)
    {
      startEncoderVal = AlexEncoder::counter - (CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT - 1);
      resultMenuItemIndex = CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT - 1;
    }

    if (resultMenuItemIndex < 0)
    {
      resultMenuItemIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    displayManager.drawCurtainMovementSelectionScreen(resultMenuItemIndex);

    if (button.isClicked())
    {
      curtainMovement = (CurtainMovement)resultMenuItemIndex;
      drawMeasuringScreen();
      return;
    }
  }
} */

/* void drawLightCheckScreen()
{
  uint32_t lastTime = millis();

  sensor0BlinkCount = 0;
  sensor1BlinkCount = 0;
  bool isLightGood = false;
  uint16_t sensor0ResSignalLevel = 0;
  uint16_t sensor1ResSignalLevel = 0;

  adcISRFlow = AdcISRFlow::SENSOR_READINGS_CHECK;
  setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
  startADCconversion(); // start first ADC conversion

  while (true)
  {
    if ((int64_t)millis() - (int64_t)lastTime > SENSOR_VALUES_UPDATE_INTERVAL)
    {
      // Serial.println(sensor0BlinkCount);
      isLightGood = sensor0BlinkCount < SENSOR_MAX_BLINK_COUNT_PER_INTERVAL &&
                    sensor1BlinkCount < SENSOR_MAX_BLINK_COUNT_PER_INTERVAL;

      if (sensor0BlinkCount >= SENSOR_MAX_BLINK_COUNT_PER_INTERVAL)
      {
        sensor0ResSignalLevel = 0;

        for (uint8_t i = 0; i < SENSOR_MAX_AVG_ARRAY_SIZE; i++)
        {
          sensor0ResSignalLevel += sensor0MaxArr[i];
        }

        sensor0ResSignalLevel /= SENSOR_MAX_AVG_ARRAY_SIZE;
      }
      else
      {
        sensor0ResSignalLevel = sensor0SignalLevel;
      }

      if (sensor1BlinkCount >= SENSOR_MAX_BLINK_COUNT_PER_INTERVAL)
      {
        sensor1ResSignalLevel = 0;

        for (uint8_t i = 0; i < SENSOR_MAX_AVG_ARRAY_SIZE; i++)
        {
          sensor1ResSignalLevel += sensor1MaxArr[i];
        }

        sensor1ResSignalLevel /= SENSOR_MAX_AVG_ARRAY_SIZE;
      }
      else
      {
        sensor1ResSignalLevel = sensor1SignalLevel;
      }

      sensor0BlinkCount = 0;
      sensor1BlinkCount = 0;

      lastTime = millis();
    }

    displayManager.drawLightCheckScreen(isLightGood, sensor0ResSignalLevel, sensor1ResSignalLevel);
    // Serial.println(sensor0Readings);

    if (button.isClicked())
    {
      adcISRFlow = AdcISRFlow::NONE;
      return;
    }
  }
} */

void drawAboutScreen()
{
  display->fillScreen(BLACK);

  while (true)
  {
    // displayManager.drawAboutScreen();

    drawStringHCentered("About this device", 15);

    display->setCursor(10, 35);
    display->printf("Hardware version: %s", About::HW_VERSION);
    display->setCursor(10, 50);
    display->printf("Firmware version: %s", About::SW_VERSION);

    if (button.isClicked())
    {
      return;
    }
  }
}

/* void drawViewRecordsScreen()
{
  while (true)
  {
    int32_t selectedRecordNumber = drawMeasurementResultRecordSelectionScreen();

    if (selectedRecordNumber == 0) //"Back" option selected
    {
      return;
    }

    StoredMeasuredResult selectedResult = getMeasurementStoredResultByNumber(selectedRecordNumber);

    if (selectedResult.recordNumber == -1)
    {
      halt(F("Halted. selectedRecordNumber is not found"));
    }

    MeasuredResult resultToDisplay = {};
    resultToDisplay.sensor0Time = selectedResult.sensor0Time;
    resultToDisplay.sensor1Time = selectedResult.sensor1Time;
    resultToDisplay.curtain1spanAspeed = selectedResult.curtain1spanAspeed;
    resultToDisplay.curtain1spanAtime = selectedResult.curtain1spanAtime;
    resultToDisplay.curtain1TotalTime = selectedResult.curtain1TotalTime;
    resultToDisplay.curtain2spanAspeed = selectedResult.curtain2spanAspeed;
    resultToDisplay.curtain2spanAtime = selectedResult.curtain2spanAtime;
    resultToDisplay.curtain2TotalTime = selectedResult.curtain2TotalTime;
    resultToDisplay.slitWidthSensor0 = selectedResult.slitWidthSensor0;
    resultToDisplay.slitWidthSensor1 = selectedResult.slitWidthSensor1;
    resultToDisplay.slitWidthAverage = selectedResult.slitWidthAverage;
    resultToDisplay.usedSensorType = selectedResult.usedSensorType;
    resultToDisplay.selectedCurtainMovement = selectedResult.selectedCurtainMovement;

    int16_t startEncoderVal = AlexEncoder::counter;

    while (true)
    {
      int16_t resultPageIndex = AlexEncoder::counter - startEncoderVal;

      if (resultPageIndex > RESULT_PAGES_COUNT - 1)
      {
        startEncoderVal = AlexEncoder::counter - (RESULT_PAGES_COUNT - 1);
        resultPageIndex = RESULT_PAGES_COUNT - 1;
      }

      if (resultPageIndex < 0)
      {
        resultPageIndex = 0;
        startEncoderVal = AlexEncoder::counter;
      }

      displayManager.drawMeasuredScreen(resultPageIndex, resultToDisplay);

      if (button.isClicked())
      {
        const char *options[] = {
            (const char *)F("Go back to list"),
            (const char *)F("Go to main menu"),
            (const char *)F("Delete record")};
        int16_t selectedOptionIndex = drawMessageScreen_P((const char *)F("What's next?"), 3, options);

        if (selectedOptionIndex == 0) // Go back to list
        {
          break;
        }

        if (selectedOptionIndex == 1) // Go to main menu
        {
          return;
        }

        if (selectedOptionIndex == 2) // Delete record
        {
          const char *options[] = {
              (const char *)F("Yes"),
              (const char *)F("No")};
          int16_t deleteOptionIndex = drawMessageScreen_P((const char *)F("Really delete?"), 2, options);

          if (deleteOptionIndex == 0) // Delete record
          {
            bool deleteRes = deleteMeasurementStoredResultByNumber(selectedResult.recordNumber);
            char *msg;

            if (!deleteRes)
            {
              msg = (char *)F("Failed to delete record");
            }
            else
            {
              msg = (char *)F("Record deleted");
            }

            const char *failedDeleteOptions[] = {(const char *)F("OK")};
            drawMessageScreen_P(msg, 1, failedDeleteOptions);
          }
          break; // go to records list selection
        }

        return;
      }
    }
  }
} */

void drawMainMenu()
{
  int16_t startEncoderVal = AlexEncoder::counter;
  Rect menuItemsRects[MAIN_MENU_ITEMS_COUNT];
  int16_t menuItemsY[MAIN_MENU_ITEMS_COUNT];

  while (true)
  {
    display->fillScreen(BLACK);

    for (uint8_t i = 0, y = 45; i < MAIN_MENU_ITEMS_COUNT; i++, y += 15)
    {
      menuItemsRects[i] = drawStringHCentered(GetMenuItemTitle((MainMenuItems)i), y);
      menuItemsY[i] = y;
    }

    int8_t resultMenuItemIndex = 0;
    int8_t prevSelectedMenuItemIndex = -1;

    while (true)
    {
      resultMenuItemIndex = AlexEncoder::counter - startEncoderVal;

      if (resultMenuItemIndex > MAIN_MENU_ITEMS_COUNT - 1)
      {
        startEncoderVal = AlexEncoder::counter - (MAIN_MENU_ITEMS_COUNT - 1);
        resultMenuItemIndex = MAIN_MENU_ITEMS_COUNT - 1;
      }

      if (resultMenuItemIndex < 0)
      {
        resultMenuItemIndex = 0;
        startEncoderVal = AlexEncoder::counter;
      }

      // displayManager.drawMainMenu(resultMenuItemIndex);
      if (prevSelectedMenuItemIndex != resultMenuItemIndex)
      {
        if (prevSelectedMenuItemIndex != -1)
        {
          Rect rOld = menuItemsRects[prevSelectedMenuItemIndex];
          display->fillRect(rOld.x, rOld.y /* - rOld.height */, rOld.width, rOld.height, BLACK);
          menuItemsRects[prevSelectedMenuItemIndex] =
              drawStringHCentered(GetMenuItemTitle((MainMenuItems)prevSelectedMenuItemIndex), menuItemsY[prevSelectedMenuItemIndex]);
        }

        String newSelMenuItem = "-> " + String(GetMenuItemTitle((MainMenuItems)resultMenuItemIndex)) + " <-";
        Rect rNew = menuItemsRects[resultMenuItemIndex];
        display->fillRect(rNew.x, rNew.y /* - rNew.height */, rNew.width, rNew.height, BLACK);

        menuItemsRects[resultMenuItemIndex] = drawStringHCentered(newSelMenuItem, menuItemsY[resultMenuItemIndex]);
        prevSelectedMenuItemIndex = resultMenuItemIndex;
      }

      if (button.isClicked())
      {
        switch ((MainMenuItems)resultMenuItemIndex)
        {
        case MainMenuItems::MEASURE:
        {
          // drawCurtainMovementSelectionScreen();
          break;
        }
        case MainMenuItems::CHECK_LIGHT:
        {
          // drawLightCheckScreen();
          break;
        }
        case MainMenuItems::CREDITS:
        {
          drawAboutScreen();
          break;
        }
        case MainMenuItems::MEASURMENT_HISTORY:
        {
          // drawViewRecordsScreen();
          break;
        }

        default:
          halt("Halted. Unknown menu item");
          break;
        }

        startEncoderVal = AlexEncoder::counter;
        break;//breack from while loop
      }
    }
  }
}

void setup() {
  // Serial.begin(115200);
  // while (!Serial);

  // display->begin();
  // display->fillScreen(BLACK);
  // display->setTextColor(WHITE);
  // // display->setCursor(10, 10);
  // // display->print("Test");
  // display->setFont(u8g2_font_6x13_tf);

  AlexEncoder::init(ENCODER_A_PIN, ENCODER_B_PIN);

  auto timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, US_IN_SECOND / USER_INPUT_POLLING_FREQ_HZ, true, 0);

  // while (true){

  //   // Serial.println(AlexEncoder::counter);
  //   ESP_LOGI(TAG, "Counter: %i, button: %i", AlexEncoder::counter, (uint8_t)button.isClicked());

  //   delay(200);
  // }
}

void loop() {

  drawMainMenu();
}

//---------------------------ADC test code

// #define EXAMPLE_ADC_UNIT ADC_UNIT_1
// #define _EXAMPLE_ADC_UNIT_STR(unit) #unit
// #define EXAMPLE_ADC_UNIT_STR(unit) _EXAMPLE_ADC_UNIT_STR(unit)
// #define EXAMPLE_ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
// #define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_11
// #define EXAMPLE_ADC_BIT_WIDTH SOC_ADC_DIGI_MAX_BITWIDTH

// #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
// #define EXAMPLE_ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
// #define EXAMPLE_ADC_GET_CHANNEL(p_data) ((p_data)->type1.channel)
// #define EXAMPLE_ADC_GET_DATA(p_data) ((p_data)->type1.data)
// #else
// #define EXAMPLE_ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
// #define EXAMPLE_ADC_GET_CHANNEL(p_data) ((p_data)->type2.channel)
// #define EXAMPLE_ADC_GET_DATA(p_data) ((p_data)->type2.data)
// #endif

// #define EXAMPLE_READ_LEN 256

// #if CONFIG_IDF_TARGET_ESP32
// static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
// #else
// static adc_channel_t channel[2] = {ADC_CHANNEL_2, ADC_CHANNEL_3};
// #endif

// static TaskHandle_t s_task_handle;
// static const char *TAG = "EXAMPLE";

// static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
// {
//   BaseType_t mustYield = pdFALSE;
//   // Notify that ADC continuous driver has done enough number of conversions
//   vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

//   return (mustYield == pdTRUE);
// }

// static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
// {
//   adc_continuous_handle_t handle = NULL;

//   adc_continuous_handle_cfg_t adc_config = {
//       .max_store_buf_size = 1024,
//       .conv_frame_size = EXAMPLE_READ_LEN,
//   };
//   ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

//   adc_continuous_config_t dig_cfg = {
//       .sample_freq_hz = 80 * 1000,
//       .conv_mode = EXAMPLE_ADC_CONV_MODE,
//       .format = EXAMPLE_ADC_OUTPUT_TYPE,
//   };

//   adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
//   dig_cfg.pattern_num = channel_num;
//   for (int i = 0; i < channel_num; i++)
//   {
//     adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
//     adc_pattern[i].channel = channel[i] & 0x7;
//     adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
//     adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

//     ESP_LOGI(TAG, "adc_pattern[%d].atten is :%" PRIx8, i, adc_pattern[i].atten);
//     ESP_LOGI(TAG, "adc_pattern[%d].channel is :%" PRIx8, i, adc_pattern[i].channel);
//     ESP_LOGI(TAG, "adc_pattern[%d].unit is :%" PRIx8, i, adc_pattern[i].unit);
//   }
//   dig_cfg.adc_pattern = adc_pattern;
//   ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

//   *out_handle = handle;
// }

// void setup()
// {
  
// }

// void loop()
// {
//   esp_err_t ret;
//   uint32_t ret_num = 0;
//   uint8_t result[EXAMPLE_READ_LEN] = {0};
//   memset(result, 0xcc, EXAMPLE_READ_LEN);

//   const uint16_t resArrLength = 1000;
//   int16_t resArr[resArrLength] = {};
//   int16_t resArrInd = 0;
//   int16_t lastVal = 0;

//   s_task_handle = xTaskGetCurrentTaskHandle();

//   adc_continuous_handle_t handle = NULL;
//   continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

//   adc_continuous_evt_cbs_t cbs = {
//       .on_conv_done = s_conv_done_cb,
//   };
//   ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
//   ESP_ERROR_CHECK(adc_continuous_start(handle));

//   while (1)
//   {

//     /**
//      * This is to show you the way to use the ADC continuous mode driver event callback.
//      * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
//      * However in this example, the data processing (print) is slow, so you barely block here.
//      *
//      * Without using this event callback (to notify this task), you can still just call
//      * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
//      */
//     // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

//     char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

//     while (1)
//     {
//       ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 20000000);
//       if (ret == ESP_OK)
//       {
//         // ESP_LOGI("TASK", "ret is %x, ret_num is %" PRIu32 " bytes", ret, ret_num);
//         for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES)
//         {
//           adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
//           uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
//           uint32_t data = EXAMPLE_ADC_GET_DATA(p);
//           // /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
//           // if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT))
//           // {
//           // ESP_LOGI(TAG, "Unit: %s, Channel: %" PRIu32 ", Value: %" PRIu32, unit, chan_num, data);
//           // }
//           // else
//           // {
//           //   ESP_LOGW(TAG, "Invalid data [%s_%" PRIu32 "_%" PRIx32 "]", unit, chan_num, data);
//           // }

//           if (chan_num == ADC_CHANNEL_2)
//           {
//             // printf("$%d;\n", p->type1.data);

//             lastVal = data;

//             if (data > 500)
//             {
//               resArr[resArrInd] = data;
//               resArrInd++;

//               if (resArrInd > resArrLength)
//               {
//                 break;
//               }
//             }
//           }
//         }

//         if ((resArrInd > 0 && lastVal < 450) || resArrInd > resArrLength)
//         {
//           break;
//         }

//         /**
//          * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
//          * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
//          * usually you don't need this delay (as this task will block for a while).
//          */
//         vTaskDelay(1);
//       }
//       else if (ret == ESP_ERR_TIMEOUT)
//       {
//         // We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
//         break;
//       }
//     }

//     for (size_t i = 0; i < resArrInd; i++)
//     {
//       printf("$%d;\n", resArr[i]);
//     }

//     while (1)
//     {
//     }
//   }

//   ESP_ERROR_CHECK(adc_continuous_stop(handle));
//   ESP_ERROR_CHECK(adc_continuous_deinit(handle));
// }

// ---------------------------ADC test code end
