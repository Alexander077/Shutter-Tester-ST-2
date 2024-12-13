#include <Arduino.h>
#include <GyverTimers.h>
// #include <U8g2lib.h>
#include <EEPROM.h>
#include <InterpolationLib.h>
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
#define SHUTTER_OPEN_LEVEL 70
#define SHUTTER_CLOSED_LEVEL 70
#define USER_INPUT_POLLING_FREQ_HZ 100
// #define SPLASH_SCREEN_VISIBLE_TIME_MS 1000000000
#define SPLASH_SCREEN_VISIBLE_TIME_MS 1250
#define SCREEN_WIDTH 128
#define CHAR_BUF_SIZE 30

#define FRAME_WINDOW_HEIGHT_35MM_SYSTEM_MM 24
#define FRAME_WINDOW_WIDTH_35MM_SYSTEM_MM 36
#define VERTICAL_HOLE_DISTANCE_MM 10.6
#define HORISONTAL_HOLE_DISTANCE_MM 16

#define MM_IN_M 1000
#define US_IN_SECOND 1000000
#define US_IN_MILLISECOND 1000

#define HW_VERSION "1.0"
#define SW_VERSION "1.0"

#define MAIN_MENU_ITEMS_COUNT 4
#define CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT 3

#define SENSOR_TYPE_CODE_PIN A7

//EEPROM memory layout: |saved measures (0-1012)|saved brightness value(1013-1022)|first run value(1023)|
#define EEPROM_FIRST_RUN_VAL_INDEX 1023
#define EEPROM_FIRST_RUN_VAL 22
#define EEPROM_LIGHT_BRIGHTNESS_START_VAL 0
#define EEPROM_LIGHT_BRIGHTNESS_EMPTY_VAL 255
#define EEPROM_LIGHT_BRIGHTNESS_START_INDEX 1013
#define EEPROM_LIGHT_BRIGHTNESS_END_INDEX (EEPROM_FIRST_RUN_VAL_INDEX - 1)

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

enum class CurtainMovement
{
	HORISONTAL,
	VERTICAL,
	LEAF,
};

enum class MainMenuItems
{
	MEASURE,
	CHECK_LIGHT,
	MEASURMENT_HISTORY,
	CREDITS,
};

struct CurtainTimings
{
	double curtain1spanAtime; //In microseconds
	double curtain2spanAtime; // In microseconds
};

volatile long pin0shutterOpenStartTime = -1,
				  pin0shutterOpenEndTime = -1,
				  pin1shutterOpenStartTime = -1,
				  pin1shutterOpenEndTime = -1;
byte val = 0;

AdcISRFlow adcISRFlow = AdcISRFlow::NONE;
AlexButton button(BUTTON_PIN);
DisplayManager displayManager;

bool curADCpinNumber = true;
volatile byte sensor0Readings = 0;
volatile byte sensor1Readings = 0;
// volatile bool isAdcWarmedUp = false;

byte sensor0Max = 0; //volatile?
byte sensor1Max = 0; // volatile?

uint16_t sensorCheckCounter = 0;
#define SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE 1000

#define INTRPOLATION_POINTS_COUNT 7
const double adcVals[INTRPOLATION_POINTS_COUNT] = {96, 112, 130, 160, 193, 235, 250};
const double timeCorrectionVals[INTRPOLATION_POINTS_COUNT] = {120, 70, 35, -30, -70, -90, -110};

CurtainMovement curtainMovement = CurtainMovement::HORISONTAL;

enum class SensorType
{
	SingleSensor,
	Frame35mm,
};

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

const uint8_t sensorsDataArraySize = 2;

const SensorUnitData sensorsData[sensorsDataArraySize] = {
		{
				SensorType::SingleSensor,
				-1,
				-1,
				-1,
				-1,
				970,
				985
		},
		{
				SensorType::Frame35mm,
				36,
				24,
				16.0,
				10.6,
				900,
				940
		}};

ISR(ADC_vect)
{
	val = ADCH; // read 8 bit value from ADC

	switch (adcISRFlow)
	{
		case AdcISRFlow::MEASURING:
		{
			static bool measuredADCpin = true;
			measuredADCpin = curADCpinNumber;
			curADCpinNumber = !curADCpinNumber;

			// make pin change here so i won't interfere 
			//and not cause noise after i call startADCconversion() below
			setADCInputPin((byte)curADCpinNumber);

			if (!measuredADCpin)
			{
				if (val > sensor0Max)
				{
					sensor0Max = val;
				}

				if (val > SHUTTER_OPEN_LEVEL && pin0shutterOpenStartTime == -1)
				{
					pin0shutterOpenStartTime = micros();
				}

				if (val < SHUTTER_CLOSED_LEVEL && pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime == -1)
				{
					pin0shutterOpenEndTime = micros();
				}
			}
			else
			{
				if (val > sensor1Max)
				{
					sensor1Max = val;
				}

				if (val > SHUTTER_OPEN_LEVEL && pin1shutterOpenStartTime == -1)
				{
					pin1shutterOpenStartTime = micros();
				}

				if (val < SHUTTER_CLOSED_LEVEL && pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime == -1)
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
			setADCInputPin((byte)curADCpinNumber);

			if (!measuredADCpin)
			{
				sensor0Readings = val;
			}
			else
			{
				sensor1Readings = val;
			}

			// sensorCheckCounter++;

			// if (sensorCheckCounter == SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE)
			// {
			// 	return;//let screen update
			// }

			startADCconversion();
			break;
		}

		default:
			break;
	}
}

ISR(TIMER2_A)
{
	// key.tick();
	// isButtonPressed = !digitalRead(BUTTON_PIN);
	button.tick();
}

bool isFirstRun()
{
	return EEPROM.read(EEPROM_FIRST_RUN_VAL_INDEX) == EEPROM_FIRST_RUN_VAL;
}

template <typename T> bool verifiedEEPROMPut(const uint16_t index, const T &data)
{
	EEPROM.put(index, data);
	T dataForVerification;
	EEPROM.get(index, dataForVerification);
	return data == dataForVerification;
}

uint8_t getSavedLightBrightness()
{
	for (size_t i = EEPROM_LIGHT_BRIGHTNESS_END_INDEX; i >= EEPROM_LIGHT_BRIGHTNESS_START_INDEX; i--)
	{
		uint8_t eepromVal = EEPROM.read(i);

		if (eepromVal != EEPROM_LIGHT_BRIGHTNESS_EMPTY_VAL)//if data has been written
		{
			return EEPROM.read(i);
		}
	}

	return 0;//if all cells are failed
}

bool saveLightBrightness(uint8_t brightness)
{
	for (size_t i = EEPROM_LIGHT_BRIGHTNESS_END_INDEX; i >= EEPROM_LIGHT_BRIGHTNESS_START_INDEX; i--)
	{
		uint8_t eepromVal = EEPROM.read(i);

		if (eepromVal != EEPROM_LIGHT_BRIGHTNESS_EMPTY_VAL) // if data has been written
		{
			bool writeRes = verifiedEEPROMPut(i, brightness);

			if (writeRes)
			{
				return true;
			}

			// if cell is failed and we do not reach last cell yet - move value to next cell
			if (!writeRes && i != EEPROM_LIGHT_BRIGHTNESS_END_INDEX) 
			{
				return verifiedEEPROMPut(i + 1, brightness);
			}
		}
	}

	halt("Halted. Program should not get here");
}

double getCorrectedSensorValue(long rawSensorTime, uint8_t maxSensorValue)
{
	double correction = Interpolation::Linear(adcVals, timeCorrectionVals, INTRPOLATION_POINTS_COUNT, (double)maxSensorValue, false);
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
	// pin1shutterOpenStartTime = 16590476;
	// pin1shutterOpenEndTime = 16592600;
	// sensor0Max = 163;
	// sensor1Max = 167;
	// curtainMovement = CurtainMovement::HORISONTAL;

	// Serial.println(String("Sensor 0 timecodes: ") + pin0shutterOpenStartTime + "|" + pin0shutterOpenEndTime);
	// Serial.println(String("Sensor 1 timecodes: ") + pin1shutterOpenStartTime + "|" + pin1shutterOpenEndTime);
	// Serial.println(String("Sensor 0 max val.: ") + sensor0Max);
	// Serial.println(String("Sensor 1 max val.: ") + sensor1Max);

	double rawSensor0TimeTaken = pin0shutterOpenEndTime - pin0shutterOpenStartTime;
	double rawSensor1TimeTaken = pin1shutterOpenEndTime - pin1shutterOpenStartTime;
	double sensor0CorrectedTime = getCorrectedSensorValue(rawSensor0TimeTaken, sensor0Max);//in microseconds
	double sensor1CorrectedTime = getCorrectedSensorValue(rawSensor1TimeTaken, sensor1Max); // in microseconds

	// Serial.println(String("Sensor 0 time taken: ") + (sensor0CorrectedTime / 1000.0) + " ms");
	// Serial.println(String("Sensor 1 time taken: ") + (sensor1CorrectedTime / 1000.0) + " ms");

	MeasuredResult res;
	res.sensor0Time = sensor0CorrectedTime / US_IN_MILLISECOND;//in milliseconds
	res.sensor1Time = sensor1CorrectedTime / US_IN_MILLISECOND;// in milliseconds

	CurtainTimings curtainTimings;
	double sensorDistance;
	double frameSize;
	CurtainMovementDirection curtainMovementDirection = CurtainMovementDirection::TopToBottom;

	setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);//need to correctly read the sensor code
	delay(100);
	uint16_t curSensorCode = analogRead(SENSOR_TYPE_CODE_PIN);
	// uint16_t curSensorCode = 927;//TODO: mocked, comment or remove 
	// Serial.print("Sensor unit code: ");
	// Serial.println(curSensorCode);
	int8_t curSensorDataIndex = -1;

	for (size_t i = 0; i < sensorsDataArraySize; i++)
	{
		if (curSensorCode >= sensorsData[i].minAdcVal && curSensorCode <= sensorsData[i].maxAdcVal)
		{
			curSensorDataIndex = i;
		}
	}

	if (curSensorDataIndex == -1)
	{
		halt(F("Sensor data not found. Program halted"));
	}

	SensorUnitData curSensorData = sensorsData[curSensorDataIndex];

	if (curtainMovement == CurtainMovement::HORISONTAL || curtainMovement == CurtainMovement::VERTICAL)
	{
		if (pin0shutterOpenStartTime < pin1shutterOpenStartTime) // left to right or top to bottom curtans movement
		{
			curtainTimings.curtain1spanAtime = (double)(pin1shutterOpenStartTime - pin0shutterOpenStartTime);
			curtainTimings.curtain2spanAtime = (double)(pin1shutterOpenEndTime - pin0shutterOpenEndTime);
			curtainMovementDirection = curtainMovement == CurtainMovement::HORISONTAL ? 
																		CurtainMovementDirection::LeftToRight : CurtainMovementDirection::TopToBottom;
		}
		else // right to left or bottom to top curtans movement
		{
			curtainTimings.curtain1spanAtime = (double)(pin0shutterOpenStartTime - pin1shutterOpenStartTime);
			curtainTimings.curtain2spanAtime = (double)(pin0shutterOpenEndTime - pin1shutterOpenEndTime);
			curtainMovementDirection = curtainMovement == CurtainMovement::HORISONTAL ? 
																	CurtainMovementDirection::RightToLeft : CurtainMovementDirection::BottomToTop;
		}
		
		frameSize = curtainMovement == CurtainMovement::HORISONTAL ? 
										curSensorData.FrameWidth : curSensorData.FrameHeight;// in millimiters
		sensorDistance = curtainMovement == CurtainMovement::HORISONTAL ? 
												curSensorData.HorisontalSensorDistance : curSensorData.VerticalSensorDistance;// in millimiters
	}
	else
	{
		//for leaf shutters
	}

	calculateResults(res, curtainTimings, sensorDistance, frameSize, sensor0CorrectedTime, sensor1CorrectedTime);

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

		displayManager.drawMeasuredScreen(resultPageIndex, curtainMovementDirection, res);

		if (button.isClicked())
		{
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

	uint8_t savedBrightness = getSavedLightBrightness();
	int16_t startEncoderVal = AlexEncoder::counter - savedBrightness;
	const uint8_t maxLightBrightness = 100;

	adcISRFlow = AdcISRFlow::MEASURING;
	setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
	startADCconversion(); // start first ADC conversion

	while (true)
	{
		int16_t resultBrightness = AlexEncoder::counter - startEncoderVal;

		if (resultBrightness > maxLightBrightness)
		{
			startEncoderVal = AlexEncoder::counter - (maxLightBrightness);
			resultBrightness = maxLightBrightness;
		}

		if (resultBrightness < 0)
		{
			resultBrightness = 0;
			startEncoderVal = AlexEncoder::counter;
		}

		displayManager.drawMeasuringScreen(resultBrightness);

		if (button.isClicked())
		{
			adcISRFlow = AdcISRFlow::NONE;
			saveLightBrightness(resultBrightness);
			return;
		}
		else if (adcISRFlow == AdcISRFlow::MEASURING &&
						 ((pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime != -1) ||
							(pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime != -1))) // Check if at least one sensor has data
		{
			delay(500); // wait for other sensors to get data
			adcISRFlow = AdcISRFlow::NONE;
			saveLightBrightness(resultBrightness);
			drawMeasuredScreen();
			return;
		}
	}
}

void drawCurtainMovementSelectionScreen()
{
	while (true)
	{
		uint8_t curMenuItem = abs(AlexEncoder::counter) % CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT;
		displayManager.drawCurtainMovementSelectionScreen(curMenuItem);

		if (button.isClicked())
		{
			curtainMovement = (CurtainMovement)curMenuItem;
			drawMeasuringScreen();
			return;
		}
	}
}

void drawLightCheckScreen()
{
	uint8_t savedBrightness = getSavedLightBrightness();
	int16_t startEncoderVal = AlexEncoder::counter - savedBrightness; 
	const uint8_t maxLightBrightness = 100;

	adcISRFlow = AdcISRFlow::SENSOR_READINGS_CHECK;
	setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
	startADCconversion(); // start first ADC conversion

	while (true)
	{
		int16_t resultBrightness = AlexEncoder::counter - startEncoderVal;

		if (resultBrightness > maxLightBrightness)
		{
			startEncoderVal = AlexEncoder::counter - (maxLightBrightness);
			resultBrightness = maxLightBrightness;
		}

		if (resultBrightness < 0)
		{
			resultBrightness = 0;
			startEncoderVal = AlexEncoder::counter;
		}

		displayManager.drawLightCheckScreen(resultBrightness, sensor0Readings, sensor1Readings);
		// Serial.println(sensor0Readings);

		if (button.isClicked())
		{
			saveLightBrightness(resultBrightness);
			return;
		}
	}
}

void drawCreditsScreen()
{
	while (true)
	{
		int8_t curMenuItem = abs(AlexEncoder::counter) % MAIN_MENU_ITEMS_COUNT;
		displayManager.drawCreditsScreen();

		if (button.isClicked())
		{
			return;
		}
	}
}

void drawMainMenu()
{
	while (true)
	{
		int8_t curMenuItem = abs(AlexEncoder::counter) % MAIN_MENU_ITEMS_COUNT;
		displayManager.drawMainMenu(curMenuItem);

		if (button.isClicked())
		{
			switch ((MainMenuItems)curMenuItem)
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
					drawCreditsScreen();
					break;
				}

			default:
				break;
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
	delay(1000);
	Serial.begin(115200);

	Serial.println((uint16_t)sizeof(StoredMeasuredResult));
	halt();

	// write first run value/need to run once befor device shipping
	// EEPROM.write(EEPROM_FIRST_RUN_VAL_INDEX, EEPROM_FIRST_RUN_VAL);

	// if (EEPROM.read(EEPROM_FIRST_RUN_VAL_INDEX) == EEPROM_FIRST_RUN_VAL)//init EEPROM on first run
	if (false)//init EEPROM on first run
	{
		// init light brightness section in EEPROM
		EEPROM.write(EEPROM_LIGHT_BRIGHTNESS_START_INDEX, EEPROM_LIGHT_BRIGHTNESS_START_VAL);

		for (size_t i = EEPROM_LIGHT_BRIGHTNESS_START_INDEX + 1; i <= EEPROM_LIGHT_BRIGHTNESS_END_INDEX; i++)
		{
			EEPROM.write(i, EEPROM_LIGHT_BRIGHTNESS_EMPTY_VAL);
		}

		EEPROM.write(EEPROM_FIRST_RUN_VAL_INDEX, EEPROM_FIRST_RUN_VAL);
	}

	ADCSRA = 0; // clear ADCSRA register
	ADCSRB = 0; // clear ADCSRB register

	// set analig pin 0 as input
	setADCInputPin(0);

	ADMUX |= (1 << REFS0); // set reference voltage
	ADMUX |= (1 << ADLAR); // left align ADC value to 8 bits from ADCH register

	setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
	// setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);

	setADCautoTriggerEnabled(false);
	ADCSRA |= (1 << ADEN); // enable ADC
	enableADCinterrupt();

	pinMode(TEST_PIN, OUTPUT);

	bool isVersionTextVisible = button.isDown();

	//Place button tate polling timer here so that button 
	//don't react while showing startup screen
	Timer2.setFrequency(USER_INPUT_POLLING_FREQ_HZ);
	Timer2.enableISR(CHANNEL_A);

	AlexEncoder::init(2, 3);
	displayManager.Init();
}

void loop()
{
	// drawMeasuredScreen();
	drawMainMenu();
}