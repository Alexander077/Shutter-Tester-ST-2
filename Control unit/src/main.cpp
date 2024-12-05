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

#define MIN_SIGNAL_LEVEL 95
#define MAX_SIGNAL_LEVEL 247

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
#define RESULT_PAGES_COUNT 7

#define EEPROM_FIRST_RUN_VAL 22
#define EEPROM_FIRST_RUN_VAL_INDEX 1023
#define EEPROM_LIGHT_BRIGHTNESS_INDEX 0

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
	double curtain1spanAtime;
	double curtain2spanAtime;
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

uint8_t getSavedLightBrightness()
{
	return EEPROM.read(EEPROM_LIGHT_BRIGHTNESS_INDEX);
}

void saveLightBrightness(uint8_t brightness)
{
	EEPROM.update(EEPROM_LIGHT_BRIGHTNESS_INDEX, brightness);
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

void calculateResults(MeasuredResult &res, const CurtainTimings curtainTimings, double sensorDistance, double frameSize/* , double sensor1time, double sensor2time */)
{
	// First curtain
	res.curtain1spanAspeed = getCurtainSpeed(curtainTimings.curtain1spanAtime, sensorDistance);
	res.curtain1spanAtime = curtainTimings.curtain1spanAtime / US_IN_MILLISECOND;
	// Serial.println("Curtain 1 span A speed: " + String(firstCurtainSpanASpeedInMPerS) + " m/s");

	res.curtain1TotalTime = frameSize / (res.curtain1spanAspeed / 1000.0);

	// Second curtain
	res.curtain2spanAspeed = getCurtainSpeed(curtainTimings.curtain2spanAtime, sensorDistance);
	res.curtain2spanAtime = curtainTimings.curtain2spanAtime / US_IN_MILLISECOND;
	// Serial.println("Curtain 1 span A speed: " + String(secondCurtainSpanASpeedInMPerS) + " m/s");

	res.curtain2TotalTime = frameSize / (res.curtain2spanAspeed / 1000.0);

	//TODO: check slit width calculation
	// // Slit width
	// double spanASlitSizeInMmByCurtain1 = (VERTICAL_HOLE_DISTANCE_MM / curtainTimings.curtain1spanAtime) * sensor1time;
	// double spanASlitSizeInMmByCurtain2 = (VERTICAL_HOLE_DISTANCE_MM / curtainTimings.curtain2spanAtime) * sensor1time;
	// // Serial.print("Span A c1: ");
	// // Serial.println(spanASlitSizeInMmByCurtain1);
	// // Serial.print("Span A c2: ");
	// // Serial.println(spanASlitSizeInMmByCurtain2);
	// res.slitWidthSpanA = (spanASlitSizeInMmByCurtain1 + spanASlitSizeInMmByCurtain2) / 2.0;

	// double spanBSlitSizeInMmByCurtain2 = (VERTICAL_HOLE_DISTANCE_MM / curtainTimings.curtain2spanBtime) * sensor2time;
	// double spanBSlitSizeInMmByCurtain1 = (VERTICAL_HOLE_DISTANCE_MM / curtainTimings.curtain1spanBtime) * sensor2time;
	// // Serial.print("Span B c1: ");
	// // Serial.println(spanBSlitSizeInMmByCurtain1);
	// // Serial.print("Span B c2: ");
	// // Serial.println(spanBSlitSizeInMmByCurtain2);
	// res.slitWidthSpanB = (spanBSlitSizeInMmByCurtain1 + spanBSlitSizeInMmByCurtain2) / 2.0;
}

void drawMeasuredScreen()
{
	/*
		Shutter result screen
			shutter time im us/ms (e.g. 1.55 ms)
			shutter speed in fraction of a sec. (e.g. 1/750 sec)
			Distance from closest shutter speeds in stops (1/2 stop from 1/500 and )
			Distance from closest shutter speeds graphically
			Slit size in mm (e.g. 1.5mm)

		Total result screen
			1 to 2 sensor first curtain travel time in m/s
			1 to 2 sensor second curtain travel time in m/s

			1 to 2 sensor first curtain speed in m/s
			1 to 2 sensor second curtain speed in m/s

			1-st curtain avg speed in m/s
			2-st curtain avg speed in m/s
			
			1-st curtain total travel time in ms
			2-st curtain total travel time in ms

			exposure unevannes text or graphical
	 */

	// Sensor 0 timecodes: 56921276|56922708
	// Sensor 1 timecodes: 56923936|56925452
	// Sensor 0 max val.: 193
	// Sensor 1 max val.: 195

	// pin0shutterOpenStartTime = 56921276;
	// pin0shutterOpenEndTime = 56922708;
	// pin1shutterOpenStartTime = 56923936;
	// pin1shutterOpenEndTime = 56925452;
	
	// sensor0Max = 193;
	// sensor1Max = 195;
	// curtainMovement = CurtainMovement::VERTICAL;

	Serial.println(String("Sensor 0 timecodes: ") + pin0shutterOpenStartTime + "|" + pin0shutterOpenEndTime);
	Serial.println(String("Sensor 1 timecodes: ") + pin1shutterOpenStartTime + "|" + pin1shutterOpenEndTime);
	// Serial.println(String("Sensor 0 max val.: ") + sensor0Max);
	// Serial.println(String("Sensor 1 max val.: ") + sensor1Max);

	double rawSensor0TimeTaken = pin0shutterOpenEndTime - pin0shutterOpenStartTime;
	double rawSensor1TimeTaken = pin1shutterOpenEndTime - pin1shutterOpenStartTime;
	double sensor0CorrectedTime = getCorrectedSensorValue(rawSensor0TimeTaken, sensor0Max);
	double sensor1CorrectedTime = getCorrectedSensorValue(rawSensor1TimeTaken, sensor1Max);

	// Serial.println(String("Sensor 0 time taken: ") + (sensor0CorrectedTime / 1000.0) + " ms");
	// Serial.println(String("Sensor 1 time taken: ") + (sensor1CorrectedTime / 1000.0) + " ms");

	// sensor0CorrectedTime = 1.34;
	// sensor1CorrectedTime = 1.38;

	MeasuredResult res;
	res.sensor0Time = sensor0CorrectedTime / US_IN_MILLISECOND;
	res.sensor1Time = sensor1CorrectedTime / US_IN_MILLISECOND;

	res.curtain1FrameAvgSpeed = 0;
	res.curtain2FrameAvgSpeed = 0;
	// res.curtain1spanAspeed = 1.22;
	// res.curtain1spanBspeed = 1.23;
	// res.curtain1spanCspeed = 1.24;
	// res.curtain1spanAtime = 1.25;
	// res.curtain1spanBtime = 1.26;
	// res.curtain1spanCtime = 1.27;
	// res.curtain1FrameAvgSpeed = 1.28;
	// res.curtain1TotalTime = 1.29;

	// res.curtain2spanAspeed = 1.32;
	// res.curtain2spanBspeed = 1.33;
	// res.curtain2spanCspeed = 1.34;
	// res.curtain2spanAtime = 1.35;
	// res.curtain2spanBtime = 1.36;
	// res.curtain2spanCtime = 1.37;
	// res.curtain2FrameAvgSpeed = 1.38;
	// res.curtain2TotalTime = 1.39;

	CurtainTimings curtainTimings;
	double sensorDistance;
	double frameSize;
	CurtainMovementDirection curtainMovementDirection = CurtainMovementDirection::TopToBottom;

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
		
		frameSize = (curtainMovement == CurtainMovement::HORISONTAL ? 
																		(double)FRAME_WINDOW_WIDTH_35MM_SYSTEM_MM : (double)FRAME_WINDOW_HEIGHT_35MM_SYSTEM_MM) / (double)MM_IN_M;
		sensorDistance = curtainMovement == CurtainMovement::HORISONTAL ? HORISONTAL_HOLE_DISTANCE_MM : VERTICAL_HOLE_DISTANCE_MM;
	}
	else
	{
		//for leaf shutters
	}

	calculateResults(res, curtainTimings, sensorDistance, frameSize/* , sensor1CorrectedTime, sensor2CorrectedTime */);

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

	adcISRFlow = AdcISRFlow::MEASURING;
	// setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
	startADCconversion(); // start first ADC conversion

	while (true)
	{
		// int8_t curMenuItem = abs(AlexEncoder::counter) % MAIN_MENU_ITEMS_COUNT;
		displayManager.drawMeasuringScreen();

		if (button.isClicked())
		{
			adcISRFlow = AdcISRFlow::NONE;
			// setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
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
	uint8_t savedBrightness = 10;// getSavedLightBrightness();
	int16_t startEncoderVal = AlexEncoder::counter - savedBrightness; //5 - 10 = -5
	const uint8_t maxLightBrightness = 100;
	// int8_t oldBrightness = savedBrightness;
	// long lastEEPROMUpdateTime = millis();
	// const uint16_t EEPROMUpdateDelayMs = 700;


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

		displayManager.drawLightCheckScreen(resultBrightness, sensor0Readings / (double)MAX_SIGNAL_LEVEL, sensor1Readings / (double)MAX_SIGNAL_LEVEL);

		// if (resultBrightness != oldBrightness && millis() - lastEEPROMUpdateTime > EEPROMUpdateDelayMs)
		// {
		// 	oldBrightness = resultBrightness;
		// 	saveLightBrightness(resultBrightness);
		// 	lastEEPROMUpdateTime = millis();
		// }

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
	// startADCconversion(); // start first ADC conversion

	// pinMode(buttonPin, INPUT_PULLUP);
	pinMode(TEST_PIN, OUTPUT);

	bool isVersionTextVisible = button.isDown();

	// Serial.println("Setup done");
	// EEPROM.write(EEPROM.length() - 3, 1);
	// EEPROM.write(EEPROM.length() - 2, 1);
	// EEPROM.write(EEPROM.length() - 1, 1);
	// EEPROM.write(EEPROM.length(), 8);

	// Serial.println(EEPROM.read(EEPROM.length()));

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