#include <Arduino.h>
#include <GyverTimers.h>
// #include <U8g2lib.h>
#include <EEPROM.h>
#include <InterpolationLib.h>
#include <AceSorting.h>
// #include <OneWire.h>
// #include <DallasTemperature.h>
// #include "../../include/LibPrintf/src/LibPrintf.h"
// #include "../../include/LibPrintf/extras/printf/printf.h"
#include "utils.h"
#include "AlexButton.h"
#include "../../include/AlexEncoder.h"
#include "DisplayManager.h"
#include "MeasuredResult.h"
#include "StoredMeasuredResult.h"
#include "CurtainMovementDirection.h"

// #define SHUTTER_TESTER_DEBUG

#define BUTTON_PIN 4
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
#define EEPROM_FIRST_RUN_VAL 22

#define EEPROM_MEASURED_RES_START_INDEX 0
#define EEPROM_MEASURED_RES_END_INDEX 1012
#define EEPROM_MEASURED_RES_TOTAL_BYTES (EEPROM_MEASURED_RES_END_INDEX - EEPROM_MEASURED_RES_START_INDEX)

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
	double curtain1spanAtime; //In microseconds
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
AlexButton button(BUTTON_PIN);
DisplayManager displayManager;
// Setup a oneWire instance to communicate with temp sensor
// OneWire oneWire(TEMP_SENSOR_PIN);
// Pass our oneWire reference to Dallas Temperature sensor
// DallasTemperature tempSensor(&oneWire);

bool curADCpinNumber = true;
volatile uint8_t sensor0SignalLevel = 0;
volatile uint8_t sensor1SignalLevel = 0;
volatile uint16_t sensor0BlinkCount = 0;
volatile uint16_t sensor1BlinkCount = 0;

//3 times per second
#define SENSOR_VALUES_UPDATE_INTERVAL 333
#define SENSOR_MAX_AVG_ARRAY_SIZE 5
#define SENSOR_MAX_BLINK_COUNT_PER_INTERVAL 4
uint8_t sensor0Max = 0; //volatile?
uint8_t sensor1Max = 0; // volatile?
uint8_t sensor0MaxArr[SENSOR_MAX_AVG_ARRAY_SIZE] = {};
uint8_t sensor1MaxArr[SENSOR_MAX_AVG_ARRAY_SIZE] = {};

uint16_t sensorCheckCounter = 0;
#define SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE 1000

#define INTRPOLATION_POINTS_COUNT 4
const double sensorMaxAdcVals[INTRPOLATION_POINTS_COUNT] = {80, 110, 140, 230};
const double timeCorrectionVals[INTRPOLATION_POINTS_COUNT] = {15, 25, 30, 0};

CurtainMovement curtainMovement = CurtainMovement::HORISONTAL;

struct SensorUnitData
{
	SensorType Type;
	double FrameWidth; // in millimiters
	double FrameHeight; // in millimiters
	double HorisontalSensorDistance; // in millimiters
	double VerticalSensorDistance;	 // in millimiters
	uint16_t minAdcVal;
	uint16_t maxAdcVal;
};

const SensorUnitData sensorsData[sensorsDataArraySize] = {
		{SensorType::Frame35mm,
		 36,
		 24,
		 16.0,
		 10.6,
		 700,
		 840},
		{SensorType::Frame6x45,
		 60,
		 45,
		 26.67,
		 20.0,
		 200,
		 300},
		{SensorType::Frame6x6,
		 60,
		 60,
		 26.67,
		 26.67,
		 900,
		 940},
		{SensorType::Frame6x7,
		 70,
		 60,
		 31.11,
		 26.67,
		 500,
		 600}};

ISR(ADC_vect)
{
	byte val = ADCH; // read 8 bit value from ADC

	switch (adcISRFlow)
	{
		case AdcISRFlow::MEASURING:
		{
			static bool measuredADCpin = true;
			measuredADCpin = curADCpinNumber;
			curADCpinNumber = !curADCpinNumber;

			// make pin change here so i won't interfere 
			//and not cause noise after i call startADCconversion() below
			setADCInputPin((uint8_t)curADCpinNumber);

			if (!measuredADCpin)//senosor 1
			{
				if (val > sensor0Max)
				{
					sensor0Max = val;
				}

				// if (val > SHUTTER_OPEN_LEVEL && pin0shutterOpenStartTime == -1)
				if (val > SHUTTER_OPEN_LEVEL && pin0shutterOpenStartTime == -1)
				{
					pin0shutterOpenStartTime = micros();
				}

				// if (val < SHUTTER_CLOSED_LEVEL && pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime == -1)
				if (val < sensor0Max - SHUTTER_OPEN_LEVEL && pin0shutterOpenEndTime == -1)
				{
					pin0shutterOpenEndTime = micros();
				}
			}
			else//sensor 2
			{
				if (val > sensor1Max)
				{
					sensor1Max = val;
				}

				// if (val > SHUTTER_OPEN_LEVEL && pin1shutterOpenStartTime == -1)
				if (val > SHUTTER_OPEN_LEVEL && pin1shutterOpenStartTime == -1)
				{
					pin1shutterOpenStartTime = micros();
				}

				// if (val < SHUTTER_CLOSED_LEVEL && pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime == -1)
				if (val < sensor1Max - SHUTTER_OPEN_LEVEL && pin1shutterOpenEndTime == -1)
				{
					pin1shutterOpenEndTime = micros();
				}
			}
			
			startADCconversion();
			digitalWriteFast(TEST_PIN, !digitalReadFast(TEST_PIN));
			break;
		}
		case AdcISRFlow::SENSOR_READINGS_CHECK:
		{
			static bool measuredADCpin = true;
			measuredADCpin = curADCpinNumber;
			curADCpinNumber = !curADCpinNumber;

			// make pin change here so i won't interfere and not
			// cause noise after i call startADCconversion() below
			setADCInputPin((uint8_t)curADCpinNumber);

			if (!measuredADCpin) // senosor 1
			{
				// uint8_t sensor1MaxArrIndex = 0;
				sensor0SignalLevel = val;

				if (val > sensor0Max)
				{
					sensor0Max = val;
				}

				// if (val > SHUTTER_OPEN_LEVEL && pin0shutterOpenStartTime == -1)
				if (val > SHUTTER_OPEN_LEVEL && pin0shutterOpenStartTime == -1)
				{
					pin0shutterOpenStartTime = micros();
				}

				// if (val < SHUTTER_CLOSED_LEVEL && pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime == -1)
				if (val < sensor0Max - SHUTTER_OPEN_LEVEL && pin0shutterOpenEndTime == -1)
				{
					pin0shutterOpenEndTime = micros();
				}

				if (pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime != -1)
				{
					static uint8_t sensor0MaxArrIndex = 0;

					sensor0BlinkCount++;
					pin0shutterOpenStartTime = -1;
					pin0shutterOpenEndTime = -1;
					sensor0MaxArr[sensor0MaxArrIndex] = sensor0Max;
					sensor0MaxArrIndex++;
					sensor0MaxArrIndex = sensor0MaxArrIndex == SENSOR_MAX_AVG_ARRAY_SIZE ? 0 : sensor0MaxArrIndex;
					sensor0Max = 0;
				}
			}
			else // sensor 2
			{
				sensor1SignalLevel = val;

				if (val > sensor1Max)
				{
					sensor1Max = val;
				}

				// if (val > SHUTTER_OPEN_LEVEL && pin1shutterOpenStartTime == -1)
				if (val > SHUTTER_OPEN_LEVEL && pin1shutterOpenStartTime == -1)
				{
					pin1shutterOpenStartTime = micros();
				}

				// if (val < SHUTTER_CLOSED_LEVEL && pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime == -1)
				if (val < sensor1Max - SHUTTER_OPEN_LEVEL && pin1shutterOpenEndTime == -1)
				{
					pin1shutterOpenEndTime = micros();
				}

				if (pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime != -1)
				{
					static uint8_t sensor1MaxArrIndex = 0;

					sensor1BlinkCount++;
					pin1shutterOpenStartTime = -1;
					pin1shutterOpenEndTime = -1;
					sensor1MaxArr[sensor1MaxArrIndex] = sensor1Max;
					sensor1MaxArrIndex++;
					sensor1MaxArrIndex = sensor1MaxArrIndex == SENSOR_MAX_AVG_ARRAY_SIZE ? 0 : sensor1MaxArrIndex;
					sensor1Max = 0;
				}
			}

			startADCconversion();
			digitalWriteFast(TEST_PIN, !digitalReadFast(TEST_PIN));
			break;
		}
		case AdcISRFlow::NONE://flow just to stop adc conversions
		{
			break;
		}
		default:
		{
			halt(F("Halted. Unknown ADC ISR flow"));
			break;
		}
	}
}

ISR(TIMER2_A)
{
	button.tick();
}

bool isFirstRun()
{
	return EEPROM.read(EEPROM_FIRST_RUN_VAL_INDEX) == EEPROM_FIRST_RUN_VAL;
}

int16_t getMeasurementSaveRecordSize()
{
	return sizeof(StoredMeasuredResult);
}

int16_t getTotalMeasurementSaveSlotsCount()
{
	return EEPROM_MEASURED_RES_TOTAL_BYTES / getMeasurementSaveRecordSize();
}

int16_t getFreeMeasurementSaveSlotsCount()
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
}

StoredMeasuredResult getMeasurementStoredResultByNumber(int32_t recordNumber)
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
}

bool deleteMeasurementStoredResultByNumber(int32_t recordNumber)
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
}

int32_t drawMeasurementResultRecordSelectionScreen()
{
	StoredMeasuredResult tempMeasRes;
	int16_t blockSize = getMeasurementSaveRecordSize();
	int16_t totalSlotsCount = getTotalMeasurementSaveSlotsCount();
	int16_t freeRecordSlots = getFreeMeasurementSaveSlotsCount();
	int16_t existingRecordsCount = totalSlotsCount - freeRecordSlots;
	int16_t totalArraySaize = existingRecordsCount + 1;
	int32_t recordNumbers[totalArraySaize];
	recordNumbers[0] = 0;//"Back" option
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
}

MeasurementRecordSaveResult saveMesurementResult(const MeasuredResult &res, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
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

		//find oldest
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
}

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
	double estSlitSpeed = sensorDistance / ((curtainTimings.curtain1spanAtime + curtainTimings.curtain2spanAtime) / 2.0);//in mm per microsecond
	res.slitWidthSensor0 = estSlitSpeed * sensor0time;// in mm
	res.slitWidthSensor1 = estSlitSpeed * sensor1time; // in mm
	res.slitWidthAverage = (res.slitWidthSensor0 + res.slitWidthSensor1) / 2.0; // in mm
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

MeasurementSaveScreenResult drawMeasurementSaveScreen(MeasuredResult &measurementRes)
{
	int16_t freeSlotsCount = getFreeMeasurementSaveSlotsCount();
	int16_t totalRecordsCount = getTotalMeasurementSaveSlotsCount();
	int16_t menuItemsCount = totalRecordsCount == freeSlotsCount ? 3 : SAVE_MEASUREMENT_MENU_ITEMS_COUNT;
	int16_t startEncoderVal = AlexEncoder::counter;

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

		displayManager.drawMeasurementSaveScreen(resultIndex, freeSlotsCount, menuItemsCount == SAVE_MEASUREMENT_MENU_ITEMS_COUNT);

		if (button.isClicked())
		{
			MeasurementRecordSaveResult saveRes;
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

					if (selectedMesurementRecordNumber == 0)//user selected "Back" option
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
}

void drawMeasuredScreen()
{
	/*
		Shutter result screen
			shutter time im us/ms (e.g. 1.55 ms)
			shutter speed in fraction of a sec. (e.g. 1/750 sec)
			Distance from closest shutter speeds graphically

		Total result screen
			1 to 2 sensor first curtain travel time in ms
			1 to 2 sensor second curtain travel time in ms

			1 to 2 sensor first curtain speed in m/s
			1 to 2 sensor second curtain speed in m/s

			1-st curtain est. total travel time in ms
			2-st curtain est. total travel time in ms

			Slit size for first and second sensor in mm (e.g. 1.5mm)
	 */

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
		rawSensor0TimeTakenUs = pin0shutterOpenEndTime - pin0shutterOpenStartTime; // in microseconds
		correctedSensor0TimeTakenUs = getCorrectedSensorValue(rawSensor0TimeTakenUs, sensor0Max); // in microseconds
		res.sensor0Time = correctedSensor0TimeTakenUs / US_IN_MILLISECOND; // in milliseconds
		sensor0DataOk = true;
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
		rawSensor1TimeTakenUs = pin1shutterOpenEndTime - pin1shutterOpenStartTime; // in microseconds
		correctedSensor1TimeTakenUs = getCorrectedSensorValue(rawSensor1TimeTakenUs, sensor1Max); // in microseconds
		res.sensor1Time = correctedSensor1TimeTakenUs / US_IN_MILLISECOND; // in milliseconds
		sensor1DataOk = true;
	}

	CurtainTimings curtainTimings;
	double sensorDistance;
	double frameSize;

	setADCprescaler(ADCPrescaler::ADC_PRESCALER_128); // needed to correctly read the sensor code
	delay(100);
	uint16_t curSensorCode = analogRead(SENSOR_TYPE_CODE_PIN);
	// uint16_t curSensorCode = 927;//TODO: mocked, comment or remove 
	setupADC();//resetup ADC
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
		halt(F("Sensor data not found. Program halted"));
	}

	SensorUnitData curSensorData = sensorsData[curSensorDataIndex];
	res.usedSensorType = curSensorData.Type;

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
					frameSize = curSensorData.FrameWidth;										 // in millimiters
					sensorDistance = curSensorData.HorisontalSensorDistance; // in millimiters
				}
				else
				{
					frameSize = curSensorData.FrameHeight;								 // in millimiters
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

		displayManager.drawMeasuredScreen(resultPageIndex/* , curtainMovementDirection */, res);

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
}

void drawMeasuringScreen()
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
}

void drawCurtainMovementSelectionScreen()
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
}

void drawLightCheckScreen()
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
}

void drawAboutScreen()
{
	while (true)
	{
		displayManager.drawAboutScreen();

		if (button.isClicked())
		{
			return;
		}
	}
}

void drawViewRecordsScreen()
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

				if (selectedOptionIndex == 0)//Go back to list
				{
					break;
				}

				if (selectedOptionIndex == 1)//Go to main menu
				{
					return;
				}

				if (selectedOptionIndex == 2) // Delete record
				{
					const char *options[] = {
							(const char *)F("Yes"),
							(const char *)F("No")};
					int16_t deleteOptionIndex = drawMessageScreen_P((const char *)F("Really delete?"), 2, options);

					if (deleteOptionIndex == 0)//Delete record
					{
						bool deleteRes = deleteMeasurementStoredResultByNumber(selectedResult.recordNumber);
						char* msg;

						if (!deleteRes)
						{
							msg = (char *)F("Failed to delete record");
						}
						else
						{
							msg = (char *)F("Record deleted");
						}
						
						const char *failedDeleteOptions[] = {(const char*)F("OK")};
						drawMessageScreen_P(msg, 1, failedDeleteOptions);
					}
					break;//go to records list selection
				}

				return;
			}
		}
	}
}

void drawMainMenu()
{
	while (true)
	{
		int16_t startEncoderVal = AlexEncoder::counter;

		while (true)
		{
			int16_t resultMenuItemIndex = AlexEncoder::counter - startEncoderVal;

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

			displayManager.drawMainMenu(resultMenuItemIndex);

			if (button.isClicked())
			{
				switch ((MainMenuItems)resultMenuItemIndex)
				{
					case MainMenuItems::MEASURE:
					{
						drawCurtainMovementSelectionScreen();
						break;
					}
					case MainMenuItems::CHECK_LIGHT:
					{
						drawLightCheckScreen();
						break;
					}
					case MainMenuItems::CREDITS:
					{
						drawAboutScreen();
						break;
					}
					case MainMenuItems::MEASURMENT_HISTORY:
					{
						drawViewRecordsScreen();
						break;
					}

					default:
						halt(F("Halted. Unknown menu item"));
						break;
				}

				startEncoderVal = AlexEncoder::counter;
			}
		}
	}
}

void sendRawEncoder()
{
	while (true)
	{
		displayManager.sendRawEncoder(AlexEncoder::counter);

		if (button.isClicked())
		{
			return;
		}
	}
}

void setup()
{
	delay(200);
	Serial.begin(115200);

	if (button.isDown()) // factory reset
	{
		// write first run value/need to run once befor device shipping
		// EEPROM.write(EEPROM_FIRST_RUN_VAL_INDEX, EEPROM_FIRST_RUN_VAL);

		// erase EEPROM
		for (size_t i = 0; i < EEPROM.length(); i++)
		{
			EEPROM.update(i, 0);
		}

		while (button.isDown());
	}

	setupADC();

	pinMode(TEST_PIN, OUTPUT);

	//Place button state polling timer here so that button 
	//don't react while showing startup screen
	Timer2.setFrequency(USER_INPUT_POLLING_FREQ_HZ);
	Timer2.enableISR(CHANNEL_A);

	AlexEncoder::init(2, 3);
	displayManager.Init();
}

void loop()
{
	// MeasuredResult r = {
	// 		Direction::Left,
	// 		3,
	// 		4,
	// 		3,
	// 		4,
	// 		5,
	// 		6,
	// 		7,
	// 		8,
	// 		9,
	// 		12,
	// 		12};
	// drawMeasurementSaveScreen(r);
	// int32_t selectedRecordIndex = drawMeasurementResultRecordSelectionScreen();
	// Serial.println(selectedRecordIndex);
	// halt();
	// drawMeasuredScreen();
	drawMainMenu();
	// char buf[40];
	// sprintf_P(buf, (const char *)F("Record saved as '%i'"), 1234);
	// drawMessageScreen(buf);
	// drawViewRecordsScreen();
}