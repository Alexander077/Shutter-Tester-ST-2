#include <Arduino.h>
#include <GyverTimers.h>
// #include <U8g2lib.h>
#include <EEPROM.h>
#include "utils.h"
#include "AlexButton.h"
#include "../../include/AlexEncoder.h"
#include "DisplayManager.h"
#include <InterpolationLib.h>

// #define SHUTTER_TESTER_DEBUG

#define BUTTON_PIN 4
#define TEST_PIN 5
#define SHUTTER_OPEN_LEVEL 70
#define SHUTTER_CLOSED_LEVEL 70
#define BUTTON_DEBOUNCE_TIME_MS 100
#define USER_INPUT_POLLING_FREQ_HZ 100
#define SCREEN_TOP_MARGIN_PX 15
// #define SPLASH_SCREEN_VISIBLE_TIME_MS 1000000000
#define SPLASH_SCREEN_VISIBLE_TIME_MS 1250
#define SHUTTER_CLOSED_FALLING_EDGE_THRESHOLD 50
#define SCREEN_WIDTH 128
#define CHAR_BUF_SIZE 30

#define MIN_SIGNAL_LEVEL 95
#define MAX_SIGNAL_LEVEL 245

#define FRAME_WINDOW_HEIGHT_35MM_SYSTEM_MM 24
#define VERTICAL_HOLE_DISTANCE_MM 8.1
#define HORISONTAL_HOLE_DISTANCE_MM 12.57

#define MM_IN_M 1000
#define US_IN_SECOND 1000000

#define HW_VERSION "1.0"
#define SW_VERSION "1.0"

enum class AdcISRFlow
{
	NONE,
	MEASURING,
	MEASURED,
	SENSOR_READINGS_CHECK,
	PWM_LIGHT_CHECK,
	// FAST_MEASURING,
	// FAST_MEASURED
};

#define MAIN_MENU_ITEMS_COUNT 4

enum class MainMenuItems
{
	MEASURE,
	CHECK_LIGHT,
	MEASURMENT_HISTORY,
	CREDITS,
};


unsigned long numSamples = 0;
unsigned long t, t0;
volatile long pin0shutterOpenStartTime = -1,
				  pin0shutterOpenEndTime = -1,
				  pin1shutterOpenStartTime = -1,
				  pin1shutterOpenEndTime = -1,
				  pin2shutterOpenStartTime = -1,
				  pin2shutterOpenEndTime = -1;
byte val = 0;
// volatile int pin0val = -1;
// volatile int pin1val = -1;
// volatile int pin2val = -1;
bool appModeChanged = false;

AdcISRFlow adcISRFlow = AdcISRFlow::NONE;
AlexButton button(BUTTON_PIN);
DisplayManager displayManager;

byte curADCpinNumber = 0;
// bool skipAdcInterrupt = true;
volatile byte sensor0Readings = 0;
volatile byte sensor1Readings = 0;
volatile byte sensor2Readings = 0;
// volatile bool isAdcWarmedUp = false;

byte pinResultIndex = -1;
byte sensor0Max = 0; //volatile?
byte sensor1Max = 0; // volatile?
byte sensor2Max = 0; // volatile?

bool pwmCheckPrevState = false;
bool isLightQualGood = true;

byte adcBuf[256];
byte adcBufCounter = 0;

uint16_t sensorCheckCounter = 0;
#define SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE 1000

#define INTRPOLATION_POINTS_CPUNT 7
const double adcVals[INTRPOLATION_POINTS_CPUNT] = {96, 112, 130, 160, 193, 235, 250};
const double timeCorrectionVals[INTRPOLATION_POINTS_CPUNT] = {120, 70, 35, -30, -70, -90, -110};


ISR(ADC_vect)
{
	val = ADCH; // read 8 bit value from ADC

	switch (adcISRFlow)
	{
		case AdcISRFlow::MEASURING:
		{
			static byte measuredADCpinNum = 0;
			measuredADCpinNum = curADCpinNumber;

			if (curADCpinNumber == 2)
			{
				curADCpinNumber = 0;
			}
			else
			{
				curADCpinNumber++;
			}

			// make pin change here so i won't interfere 
			//and not cause noise after i call startADCconversion() below
			setADCInputPin(curADCpinNumber);

			if (measuredADCpinNum == 0)
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
			else if (measuredADCpinNum == 1)
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
			else if (measuredADCpinNum == 2)
			{
				if (val > sensor2Max)
				{
					sensor2Max = val;
				}

				if (val > SHUTTER_OPEN_LEVEL && pin2shutterOpenStartTime == -1)
				{
					pin2shutterOpenStartTime = micros();
				}

				if (val < SHUTTER_CLOSED_LEVEL && pin2shutterOpenStartTime != -1 && pin2shutterOpenEndTime == -1)
				{
					pin2shutterOpenEndTime = micros();
				}
			}

			digitalWriteFast(TEST_PIN, !digitalReadFast(TEST_PIN));
			startADCconversion();
			break;
		}
		case AdcISRFlow::SENSOR_READINGS_CHECK:
		{
			static byte measuredADCpinNum = 0;
			measuredADCpinNum = curADCpinNumber;

			if (curADCpinNumber == 2)
			{
				curADCpinNumber = 0;
			}
			else
			{
				curADCpinNumber++;
			}

			// make pin change here so i won't interfere and not
			// cause noise after i call startADCconversion() below
			setADCInputPin(curADCpinNumber);

			if (measuredADCpinNum == 0)
			{
				if (val > sensor0Max)
				{
					sensor0Max = val;
				}
			}
			else if (measuredADCpinNum == 1)
			{
				if (val > sensor1Max)
				{
					sensor1Max = val;
				}
			}
			else if (measuredADCpinNum == 2)
			{
				if (val > sensor2Max)
				{
					sensor2Max = val;
				}
			}

			sensorCheckCounter++;

			if (sensorCheckCounter == SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE)
			{
				return;//let screen update
			}

			break;
		}
		case AdcISRFlow::PWM_LIGHT_CHECK:
		{
			if (val > sensor1Max)
			{
				sensor1Max = val;
			}

			// adcBuf[adcBufCounter++] = val;

			static uint16_t counter = 0;
			static uint8_t pwmCheckCounter = 0;

			if (val >= MIN_SIGNAL_LEVEL)
			{
				if (pwmCheckPrevState == false)
				{
					pwmCheckCounter++;
					pwmCheckPrevState = true;
				}
			}
			else
			{
				if (pwmCheckPrevState == true)
				{
					pwmCheckCounter++;
					pwmCheckPrevState = false;
				}
			}

			if (pwmCheckCounter > 5)
			{
				disableADCinterrupt();
				isLightQualGood = false;
				pwmCheckCounter = 0;
			}

			if (counter == 20000)
			{
				disableADCinterrupt();
				counter = 0;
			}

			counter++;
			digitalWriteFast(TEST_PIN, !digitalReadFast(TEST_PIN));
			return;
			break;
		}

		default:
			break;
	}

	// digitalWriteFast(TEST_PIN, !digitalReadFast(TEST_PIN));

	// startADCconversion();
}

ISR(TIMER2_A)
{
	// key.tick();
	// isButtonPressed = !digitalRead(BUTTON_PIN);
	button.tick();
}

double getCorrectedSensorValue(long rawSensorTime, uint8_t maxSensorValue)
{
	double correction = Interpolation::Linear(adcVals, timeCorrectionVals, INTRPOLATION_POINTS_CPUNT, (double)maxSensorValue, false);
	double resSensorTime = rawSensorTime + correction;
	return resSensorTime;
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
			first curtain travel time
			second curtain travel time
			1 to 2 sensor first curtain speed in m/s
			2 to 3 sensor first curtain speed in m/s
			1 to 2 sensor second curtain speed in m/s
			2 to 3 sensor second curtain speed in m/s
			1-st curtain avg speed in m/s
			2-st curtain avg speed in m/s
			1-st curtain total travel time in ms
			2-st curtain total travel time in ms

			exposure unevannes text or graphical
	 */

	Serial.println(String("Sensor 0 timecodes: ") + pin0shutterOpenStartTime + "|" + pin0shutterOpenEndTime);
	Serial.println(String("Sensor 1 timecodes: ") + pin1shutterOpenStartTime + "|" + pin1shutterOpenEndTime);
	Serial.println(String("Sensor 2 timecodes: ") + pin2shutterOpenStartTime + "|" + pin2shutterOpenEndTime);

	double rawSensor0TimeTaken = pin0shutterOpenEndTime - pin0shutterOpenStartTime;
	double rawSensor1TimeTaken = pin1shutterOpenEndTime - pin1shutterOpenStartTime;
	double rawSensor2TimeTaken = pin2shutterOpenEndTime - pin2shutterOpenStartTime;
	double sensor0CorrectedTime = getCorrectedSensorValue(rawSensor0TimeTaken, sensor0Max);
	double sensor1CorrectedTime = getCorrectedSensorValue(rawSensor1TimeTaken, sensor1Max);
	double sensor2CorrectedTime = getCorrectedSensorValue(rawSensor2TimeTaken, sensor2Max);
	Serial.println(String("Sensor 0 time taken: ") + (sensor0CorrectedTime / 1000.0) + " ms");
	Serial.println(String("Sensor 1 time taken: ") + (sensor1CorrectedTime / 1000.0) + " ms");
	Serial.println(String("Sensor 2 time taken: ") + (sensor2CorrectedTime / 1000.0) + " ms");
	// double sensor0Speed = 1 / (sensor0CorrectedTime / US_IN_SECOND);
	// double sensor1Speed = 1 / (sensor1CorrectedTime / US_IN_SECOND);
	// double sensor2Speed = 1 / (sensor2CorrectedTime / US_IN_SECOND);

	sensor0CorrectedTime = 1.34;
	sensor1CorrectedTime = 1.38;
	sensor2CorrectedTime = 1.45;

	int16_t startEncoderVal = AlexEncoder::counter;

	while (true)
	{
		int16_t resultPageIndex = AlexEncoder::counter - startEncoderVal;
		Serial.println(resultPageIndex);

		switch (resultPageIndex)
		{
			case 0:
			{
				displayManager.drawMeasuredScreen(resultPageIndex, String(sensor0CorrectedTime, 2));
				break;
			}
			case 1:
			{
				displayManager.drawMeasuredScreen(resultPageIndex, String(sensor1CorrectedTime, 2));
				break;
			}
			case 2:
			{
				displayManager.drawMeasuredScreen(resultPageIndex, String(sensor2CorrectedTime, 2));
				break;
			}
			
			default:
				break;
		}

		if (button.isClicked())
		{
			return;
		}
	}

	// String pinResTitleStr = "Sensor " + String(pinResultIndex + 1);
	// String pinResStrTime1 = "";
	// String pinResStrTime2 = "";

	// bool signalLevelOk = true;

	// if (pinMaxSignalValue >= 0 && pinMaxSignalValue < MIN_SIGNAL_LEVEL)
	// {
	// 	pinResStrTime1 += "Light is too dim";
	// 	signalLevelOk = false;
	// }
	// else if (pinMaxSignalValue > MAX_SIGNAL_LEVEL)
	// {
	// 	pinResStrTime1 += "Light is too bright";
	// 	signalLevelOk = false;
	// }

	// if (signalLevelOk && pinTimeTaken)
	// {
	// 	if (pinTimeTaken < 250)
	// 	{
	// 		pinResStrTime1 += "To short";
	// 		pinResStrTime2 += "Check light";
	// 	}
	// 	else
	// 	{
	// 		float correction = Interpolation::Linear(adcVals, timeCorrectionVals, INTRPOLATION_POINTS_CPUNT, (double)pinMaxSignalValue, false);
	// 		pinTimeTaken += correction;

	// 		if (pinTimeTaken > 1000) // more than a millisecond
	// 		{
	// 			pinResStrTime1 += String(pinTimeTaken / 1000.0, 2) + " ms";
	// 		}
	// 		else // under the millisecond
	// 		{
	// 			pinResStrTime1 += String(pinTimeTaken, 0) + " us";
	// 		}

	// 		if (pinTimeTaken < 1000000) // if time is less than second
	// 		{
	// 			pinResStrTime2 += "1/" + String(1000000.0 / pinTimeTaken, 1) + " sec";
	// 		}
	// 	}
	// }

	bool horisontalShutterMovement = true;

	if (pin0shutterOpenStartTime < pin1shutterOpenStartTime && horisontalShutterMovement) // left to right curtans movement
	{
	}
	else if (pin0shutterOpenStartTime < pin1shutterOpenStartTime && !horisontalShutterMovement) // top to bottom curtans movement
	{
		double firstCurtainSensor0toSensor1TravelTime = (double)(pin1shutterOpenStartTime - pin0shutterOpenStartTime);
		double firstCurtainSpeedMmPerUs = VERTICAL_HOLE_DISTANCE_MM / firstCurtainSensor0toSensor1TravelTime;
		double firstCurtainSpeedInMmPerS = firstCurtainSpeedMmPerUs * (double) US_IN_SECOND;
		double firstCurtainSpanASpeedInMPerS = firstCurtainSpeedInMmPerS / (double) MM_IN_M;
		Serial.println("Curtain 1 span A speed: " + String(firstCurtainSpanASpeedInMPerS) + " m/s");

		double firstCurtainSensor1toSensor2TravelTime = (double)(pin2shutterOpenStartTime - pin1shutterOpenStartTime);
		firstCurtainSpeedMmPerUs = VERTICAL_HOLE_DISTANCE_MM / firstCurtainSensor1toSensor2TravelTime;
		firstCurtainSpeedInMmPerS = firstCurtainSpeedMmPerUs * (double)US_IN_SECOND;
		double firstCurtainSpanBSpeedInMPerS = firstCurtainSpeedInMmPerS / (double)MM_IN_M;
		Serial.println("Curtain 1 span B speed: " + String(firstCurtainSpanBSpeedInMPerS) + " m/s");

		double firstCurtainAvgSpeed = (firstCurtainSpanASpeedInMPerS + firstCurtainSpanBSpeedInMPerS) / 2.0;
		Serial.println("Curtain 1 avg speed: " + String(firstCurtainAvgSpeed) + " m/s");

		Serial.println("Curtain 1 total travel time: " + String(0.024 / (firstCurtainAvgSpeed / 1000.0)) + " ms");

		// double secondCurtainSensor0toSensor1TravelTime = (double)(pin1shutterOpenEndTime - pin0shutterOpenEndTime);
		// double secondCurtainSpeedMmPerUs = VERTICAL_HOLE_DISTANCE_MM / secondCurtainSensor0toSensor1TravelTime;
		// double secondCurtainSpeedInMmPerS = secondCurtainSpeedMmPerUs * (double)US_IN_SECOND;
		// double secondCurtainSpeedInMPerS = secondCurtainSpeedInMmPerS / (double)MM_IN_M;
		// Serial.println("Curtain 2 span A speed: " + String(secondCurtainSpeedInMPerS) + " m/s");

		// // Serial.println("Curtain 2 avg speed: " + String((firstCurtainSpeedInMPerS + secondCurtainSpeedInMPerS) / 2.0) + " m/s");

		// double secondCurtainSensor1toSensor2TravelTime = (double)(pin1shutterOpenEndTime - pin0shutterOpenEndTime);
		// secondCurtainSpeedMmPerUs = VERTICAL_HOLE_DISTANCE_MM / secondCurtainSensor1toSensor2TravelTime;
		// secondCurtainSpeedInMmPerS = secondCurtainSpeedMmPerUs * (double)US_IN_SECOND;
		// secondCurtainSpeedInMPerS = secondCurtainSpeedInMmPerS / (double)MM_IN_M;
		// Serial.println("Curtain 2 span B speed: " + String(secondCurtainSpeedInMPerS) + " m/s");

		// double spanASlitSizeInMmByFirstCurtain = firstCurtainSpeedMmPerUs * sensor1CorrctedTime;
		// Serial.println("Slit width at span A by 1-st curtain: " + String(spanASlitSizeInMmByFirstCurtain) + " mm");

		// double spanASlitSizeInMmBySecondCurtain = secondCurtainSpeedMmPerUs * sensor1CorrctedTime;
		// Serial.println("Slit width at span A by 2-nd curtain: " + String(spanASlitSizeInMmBySecondCurtain) + " mm");

		// double firstCurtainSensor1toSensor2TravelTime = (double)(pin2shutterOpenStartTime - pin1shutterOpenStartTime);
		// firstCurtainSpeedMmPerUs = VERTICAL_HOLE_DISTANCE_MM / firstCurtainSensor1toSensor2TravelTime;
		// firstCurtainSpeedInMmPerS = firstCurtainSpeedMmPerUs * (double)US_IN_SECOND;
		// firstCurtainSpeedInMPerS = firstCurtainSpeedInMmPerS / (double)MM_IN_M;
		// Serial.println("Curtain 1 span B speed: " + String(firstCurtainSpeedInMPerS) + " m/s");

		// double secondCurtainSensor1toSensor2TravelTime = (double)(pin2shutterOpenEndTime - pin1shutterOpenEndTime);
		// secondCurtainSpeedMmPerUs = VERTICAL_HOLE_DISTANCE_MM / firstCurtainSensor1toSensor2TravelTime;
		// secondCurtainSpeedInMmPerS = firstCurtainSpeedMmPerUs * (double)US_IN_SECOND;
		// secondCurtainSpeedInMPerS = firstCurtainSpeedInMmPerS / (double)MM_IN_M;
		// Serial.println("Curtain 2 span B speed: " + String(firstCurtainSpeedInMPerS) + " m/s");
	}
	else if (pin2shutterOpenStartTime < pin1shutterOpenStartTime && horisontalShutterMovement) // right to left curtans movement
	{
		
	}
	else if (pin2shutterOpenStartTime < pin1shutterOpenStartTime && !horisontalShutterMovement) // bottom to top curtans movement
	{
		/* code */
	}
	else
	{
		// unknown curtans movement dirction, probably a leaf shutter
	}
}

void drawMeasuringScreen()
{
	pin0shutterOpenStartTime = -1;
	pin0shutterOpenEndTime = -1;
	pin1shutterOpenStartTime = -1;
	pin1shutterOpenEndTime = -1;
	pin2shutterOpenStartTime = -1;
	pin2shutterOpenEndTime = -1;
	sensor0Max = 0;
	sensor1Max = 0;
	sensor2Max = 0;

	adcISRFlow = AdcISRFlow::MEASURING;
	// setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
	startADCconversion(); // start first ADC conversion
	displayManager.drawMeasuringScreen();

	while (true)
	{
		// int8_t curMenuItem = abs(AlexEncoder::counter) % MAIN_MENU_ITEMS_COUNT;

		if (button.isClicked())
		{
			adcISRFlow = AdcISRFlow::NONE;
			// setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
			return;
		}
		else if (adcISRFlow == AdcISRFlow::MEASURING &&
						 ((pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime != -1) ||
							(pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime != -1) ||
							(pin2shutterOpenStartTime != -1 && pin2shutterOpenEndTime != -1))) // Check if at least one sensor has data
		{
			delay(500); // wait for other sensors to get data
			pinResultIndex = 0;
			adcISRFlow = AdcISRFlow::MEASURED;
			drawMeasuredScreen();
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
					drawMeasuringScreen();
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
	Serial.begin(115200);

	#ifdef SHUTTER_TESTER_DEBUG
	Serial.begin(115200);
	#endif
	
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

	// display.begin();
	// display.setFont(u8g2_font_6x13_tf /*  u8g2_font_ncenB14_tr */);

	// pinMode(buttonPin, INPUT_PULLUP);
	pinMode(TEST_PIN, OUTPUT);

	bool isVersionTextVisible = button.isDown();

	// display.firstPage();
	// do
	// {
	// 	char buf[CHAR_BUF_SIZE];
	// 	drawStringHCentered("Shutter Tester", SCREEN_TOP_MARGIN_PX + (isVersionTextVisible ? 0 : 10));
	// 	drawStringHCentered("ST-1", SCREEN_TOP_MARGIN_PX + (isVersionTextVisible ? 13 : 30));

	// 	if (isVersionTextVisible)
	// 	{
	// 		String line = String("HW ver: ") + HW_VERSION;
	// 		line.toCharArray(buf, CHAR_BUF_SIZE);

	// 		drawStringHCentered(buf, SCREEN_TOP_MARGIN_PX + 30);
	// 		line = String("SW ver: ") + SW_VERSION;
	// 		line.toCharArray(buf, CHAR_BUF_SIZE);
	// 		drawStringHCentered(buf, SCREEN_TOP_MARGIN_PX + 43);
	// 	}

	// 	// display.drawBox(1, 1, 128, 64);
	// 	// delay(5000);
	// } while (display.nextPage());


	// delay(SPLASH_SCREEN_VISIBLE_TIME_MS);

	// wait for user to release the button
	while (button.isDown()){} 
	
	// display.clearBuffer();
	// display.clear();
	// display.clearDisplay();

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

// long counter = 0;

void loop()
{
	delay(1000);
	drawMeasuredScreen();
	drawMainMenu();

	switch (adcISRFlow)
	{
		case AdcISRFlow::NONE:
		{
			if (!appModeChanged)
			{
				// display.firstPage();
				// do
				// {
				// 	drawStringHCentered("Press the button", SCREEN_TOP_MARGIN_PX);
				// 	drawStringHCentered("to start measuring", SCREEN_TOP_MARGIN_PX + 13);
				// 	drawStringHCentered("Long press the button", SCREEN_TOP_MARGIN_PX + 30);
				// 	drawStringHCentered("to check sensors", SCREEN_TOP_MARGIN_PX + 43);
				// } while (display.nextPage());

				appModeChanged = true;
			}

			break;
		}
		case AdcISRFlow::MEASURING:
		{
			if (!appModeChanged)
			{
				// display.firstPage();
				// do
				// {
				// 	drawStringHCentered("--- MEASURING ---", SCREEN_TOP_MARGIN_PX);
				// 	drawStringHCentered("Release camera", SCREEN_TOP_MARGIN_PX + 18);
				// 	drawStringHCentered("shutter", SCREEN_TOP_MARGIN_PX + 30);
				// 	drawStringHCentered("button - cancel", SCREEN_TOP_MARGIN_PX + 45);
				// } while (display.nextPage());

				appModeChanged = true;
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
			}

			break;
		}
		/* case AdcISRFlow::FAST_MEASURING:
		{
			if (!appModeChanged)
			{
				display.firstPage();
				do
				{
					drawStringHCentered("- FAST MEASURING -", SCREEN_TOP_MARGIN_PX);
					drawStringHCentered("Release camera", SCREEN_TOP_MARGIN_PX + 18);
					drawStringHCentered("shutter", SCREEN_TOP_MARGIN_PX + 30);
					drawStringHCentered("button - cancel", SCREEN_TOP_MARGIN_PX + 45);
				} while (display.nextPage());

				appModeChanged = true;
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
				setADCautoTriggerEnabled(true);
				startADCconversion();
			}

			break;
		} */
		case AdcISRFlow::MEASURED:
		{
			if (!appModeChanged)
			{
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);

				double pinTimeTaken = -1;
				byte pinMaxSignalValue = 0;

				switch (pinResultIndex)
				{
					case 0:
					{
						if (pin0shutterOpenEndTime != -1 && pin0shutterOpenStartTime != -1)
						{
							pinTimeTaken = pin0shutterOpenEndTime - pin0shutterOpenStartTime;
							pinMaxSignalValue  = sensor0Max;
						}
						
						break;
					}
					case 1:
					{
						if (pin1shutterOpenEndTime != -1 && pin1shutterOpenStartTime != -1)
						{
							pinTimeTaken = pin1shutterOpenEndTime - pin1shutterOpenStartTime;
							pinMaxSignalValue = sensor1Max;
						}

						break;
					}
					case 2:
					{
						if (pin2shutterOpenEndTime != -1 && pin2shutterOpenStartTime != -1)
						{
							pinTimeTaken = pin2shutterOpenEndTime - pin2shutterOpenStartTime;
							pinMaxSignalValue = sensor2Max;
						}

						break;
					}

					default:
						break;
				}

				String pinResTitleStr = "Sensor " + String(pinResultIndex + 1);
				String pinResStrTime1 = "";
				String pinResStrTime2 = "";

				// if (pinTimeTaken != -1)
				// {
					bool signalLevelOk = true;

					if (pinMaxSignalValue >= 0 && pinMaxSignalValue < MIN_SIGNAL_LEVEL)
					{
						pinResStrTime1 += "Light is too dim";
						signalLevelOk = false;
					}
					else if (pinMaxSignalValue > MAX_SIGNAL_LEVEL)
					{
						pinResStrTime1 += "Light is too bright";
						signalLevelOk = false;
					}

					if (signalLevelOk && pinTimeTaken)
					{
						if (pinTimeTaken < 250)
						{
							pinResStrTime1 += "To short";
							pinResStrTime2 += "Check light";
						}
						else
						{
							float correction = Interpolation::Linear(adcVals, timeCorrectionVals, INTRPOLATION_POINTS_CPUNT, (double)pinMaxSignalValue, false);
							pinTimeTaken += correction;

							if (pinTimeTaken > 1000) // more than a millisecond
							{
								pinResStrTime1 += String(pinTimeTaken / 1000.0, 2) + " ms";
							}
							else // under the millisecond
							{
								pinResStrTime1 += String(pinTimeTaken, 0) + " us";
							}

							if (pinTimeTaken < 1000000) // if time is less than second
							{
								pinResStrTime2 += "1/" + String(1000000.0 / pinTimeTaken, 1) + " sec";
							}
						}
					}
				// }
				// else
				// {
				// 	pinResStrTime1 += "No results";//TODO: replace with "light too dim functionality"
				// }

				char res[CHAR_BUF_SIZE];

				// display.firstPage();
				// do
				// {
				// 	pinResTitleStr.toCharArray(res, CHAR_BUF_SIZE);
				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(res);
				// 	Serial.print(": ");
				// 	#endif
				// 	drawStringHCentered(res, SCREEN_TOP_MARGIN_PX);
				// 	pinResStrTime1.toCharArray(res, CHAR_BUF_SIZE);
				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(res);
				// 	Serial.print(", max: ");
				// 	Serial.println(pinMaxSignalValue);
				// 	#endif
				// 	drawStringHCentered(res, SCREEN_TOP_MARGIN_PX + 15);
				// 	pinResStrTime2.toCharArray(res, CHAR_BUF_SIZE);
				// 	drawStringHCentered(res, SCREEN_TOP_MARGIN_PX + 30);
				// 	drawStringHCentered("button - next", SCREEN_TOP_MARGIN_PX + 45);
				// } while (display.nextPage());

				appModeChanged = true;
			}

			break;
		}
		/* case AdcISRFlow::FAST_MEASURED:
		{
			if (!appModeChanged)
			{
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
				double pinTimeTaken = pin1shutterOpenEndTime - pin1shutterOpenStartTime;

				String pinResTitleStr = "Sensor 2";
				String pinResStrTime1 = "";
				String pinResStrTime2 = "";

				if (pinTimeTaken > 1000) // more than a millisecond
				{
					pinResStrTime1 += String(pinTimeTaken / 1000.0, 2) + " ms";
				}
				else // under the millisecond
				{
					pinResStrTime1 += String(pinTimeTaken, 0) + " us";
				}

				if (pinTimeTaken < 1000000) // if time is less than second
				{
					pinResStrTime2 += "1/" + String(1000000.0 / pinTimeTaken, 1) + " sec";
				}

				// const byte bufSize = 60;
				// byte buf[bufSize];

				// for (byte i = adcBufCounter, counter = 0; counter < bufSize; i--, counter++)
				// {
				// 	buf[bufSize - counter] = adcBuf[i];
				// }

				// for (byte i = 0; i < bufSize; i++)
				// {
				// 	Serial.println(buf[i]);
				// }

				char res[CHAR_BUF_SIZE];

				display.firstPage();
				do
				{
					pinResTitleStr.toCharArray(res, CHAR_BUF_SIZE);
					drawStringHCentered(res, SCREEN_TOP_MARGIN_PX);
					pinResStrTime1.toCharArray(res, CHAR_BUF_SIZE);
					drawStringHCentered(res, SCREEN_TOP_MARGIN_PX + 15);
					pinResStrTime2.toCharArray(res, CHAR_BUF_SIZE);
					drawStringHCentered(res, SCREEN_TOP_MARGIN_PX + 30);
					drawStringHCentered("button - next", SCREEN_TOP_MARGIN_PX + 45);
				} while (display.nextPage());

				appModeChanged = true;
			}

			break;
		} */
		case AdcISRFlow::SENSOR_READINGS_CHECK:
		{
			if (sensorCheckCounter == SENSOR_CHECK_COUNTER_SCREEN_UPDATE_VALUE);
			{
				// display.firstPage();
				// do
				// {
				// 	display.drawStr(2, SCREEN_TOP_MARGIN_PX, "Sensor 1: ");

				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print("Sensor 1: ");
				// 	#endif

				// 	if (sensor0Max < MIN_SIGNAL_LEVEL)
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX, "Too dim");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("Too dim");
				// 		#endif
				// 	}
				// 	else if (sensor0Max > MAX_SIGNAL_LEVEL)
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX, "Too bright");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("Too bright");
				// 		#endif
				// 	}
				// 	else
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX, "OK");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("OK");
				// 		#endif
				// 	}

				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(" (");
				// 	Serial.print(sensor0Readings);
				// 	Serial.print(")");
				// 	#endif

				// 	display.drawStr(2, SCREEN_TOP_MARGIN_PX + 15, "Sensor 2: ");
				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(", Sensor 2: ");
				// 	#endif

				// 	if (sensor1Max < MIN_SIGNAL_LEVEL)
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX + 15, "Too dim");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("Too dim");
				// 		#endif
				// 	}
				// 	else if (sensor1Max > MAX_SIGNAL_LEVEL)
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX + 15, "Too bright");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("Too bright");
				// 		#endif
				// 	}
				// 	else
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX + 15, "OK");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("OK");
				// 		#endif
				// 	}

				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(" (");
				// 	Serial.print(sensor1Readings);
				// 	Serial.print(")");
				// 	#endif

				// 	display.drawStr(2, SCREEN_TOP_MARGIN_PX + 30, "Sensor 3: ");
				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(", Sensor 3: ");
				// 	#endif

				// 	if (sensor2Max < MIN_SIGNAL_LEVEL)
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX + 30, "Too dim");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("Too dim");
				// 		#endif
				// 	}
				// 	else if (sensor2Max > MAX_SIGNAL_LEVEL)
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX + 30, "Too bright");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("Too bright");
				// 		#endif
				// 	}
				// 	else
				// 	{
				// 		display.drawStr(57, SCREEN_TOP_MARGIN_PX + 30, "OK");
				// 		#ifdef SHUTTER_TESTER_DEBUG
				// 		Serial.print("OK");
				// 		#endif
				// 	}

				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.print(" (");
				// 	Serial.print(sensor2Readings);
				// 	Serial.print(")");
				// 	#endif

				// 	#ifdef SHUTTER_TESTER_DEBUG
				// 	Serial.println();
				// 	#endif

				// 	display.drawStr(30, SCREEN_TOP_MARGIN_PX + 45, "button - next");

				// } while (display.nextPage());

				sensor0Max = 0;
				sensor1Max = 0;
				sensor2Max = 0;
				sensorCheckCounter = 0;
				startADCconversion();
			}
				break;
		}
		case AdcISRFlow::PWM_LIGHT_CHECK:
		{
			if (!appModeChanged)
			{
				appModeChanged = true;
				delay(50);
				setADCInputPin(1);
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
				setADCautoTriggerEnabled(true);
				startADCconversion();
				enableADCinterrupt();
			}
			else
			{
				// static byte prevPwmCheckCounter = pwmCheckCounter;
				// Serial.println(pwmCheckCounter);

				// display.firstPage();

				// do
				// {
				// 	drawStringHCentered("Light quality:", SCREEN_TOP_MARGIN_PX);
				// 	#define SECOND_ROW_Y 15

				// 	float signalLevel = 0;

				// 	if (sensor1Max > MIN_SIGNAL_LEVEL)
				// 	{
				// 		signalLevel = ((float)(sensor1Max - MIN_SIGNAL_LEVEL)) /
				// 				((float)(MAX_SIGNAL_LEVEL - MIN_SIGNAL_LEVEL)) * 100.0;

				// 		if (signalLevel > 100)
				// 		{
				// 			signalLevel = 100;
				// 		}
				// 	}

				// 	char res[CHAR_BUF_SIZE];
				// 	String signalLevelStr = String(signalLevel, 0);
				// 	signalLevelStr.toCharArray(res, CHAR_BUF_SIZE);

				// 	if (sensor1Max >= MIN_SIGNAL_LEVEL/*  && sensor1Max <= MAX_SIGNAL_LEVEL */)
				// 	{
				// 		// if (abs(pwmCheckCounter - prevPwmCheckCounter) > 3)
				// 		// {
				// 		// 	drawStringHCentered("Bad light", SCREEN_TOP_MARGIN_PX + 20);
				// 		// }
				// 		// else
				// 		// {
				// 		// 	drawStringHCentered("OK", SCREEN_TOP_MARGIN_PX + 20);
				// 		// }

				// 		if (!isLightQualGood)
				// 		{
				// 			drawStringHCentered("Bad light", SCREEN_TOP_MARGIN_PX + SECOND_ROW_Y);
				// 		}
				// 		else
				// 		{
				// 			drawStringHCentered("OK", SCREEN_TOP_MARGIN_PX + SECOND_ROW_Y);
				// 		}

				// 	}
				// 	else
				// 	{
				// 		if (sensor1Max < MIN_SIGNAL_LEVEL)
				// 		{
				// 			drawStringHCentered("Light is too dim", SCREEN_TOP_MARGIN_PX + SECOND_ROW_Y);
				// 		}

				// 		// if (sensor1Max > MAX_SIGNAL_LEVEL)
				// 		// {
				// 		// 	drawStringHCentered("Light is too bright", SCREEN_TOP_MARGIN_PX + 20);
				// 		// }
				// 	}


				// 	display.drawStr(15, SCREEN_TOP_MARGIN_PX + 30, "Signal level:");
				// 	display.drawStr(97, SCREEN_TOP_MARGIN_PX + 30, res);

				// 	drawStringHCentered("button - main screen", SCREEN_TOP_MARGIN_PX + 45);

				// } while (display.nextPage());

				// prevPwmCheckCounter = pwmCheckCounter;
				isLightQualGood = true;
				sensor1Max = 0;

				// setADCautoTriggerEnabled(false);
				// setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);


				// setADCautoTriggerEnabled(true);
				// setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
				// delay(50);
				// startADCconversion();

				enableADCinterrupt();
			}

			break;
		}

		default:
			break;
	}

	if (button.isClicked())
	{
		switch (adcISRFlow)
		{
			case AdcISRFlow::NONE:
			{
				appModeChanged = false;
				adcISRFlow = AdcISRFlow::MEASURING;
				break;
			}
			case AdcISRFlow::MEASURING:
			{
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
				adcISRFlow = AdcISRFlow::NONE;
				appModeChanged = false;
				break;
			}
			// case AdcISRFlow::FAST_MEASURING:
			// {
			// 	setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
			// 	setADCautoTriggerEnabled(false);
			// 	startADCconversion();
			// 	adcISRFlow = AdcISRFlow::NONE;
			// 	appModeChanged = false;
			// 	break;
			// }
			case AdcISRFlow::MEASURED:
			{
				if (pinResultIndex > -1 && pinResultIndex < 2)
				{
					pinResultIndex++;
					appModeChanged = false;
				}
				else
				{
					adcISRFlow = AdcISRFlow::NONE;
					appModeChanged = false;
					setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
					pin0shutterOpenStartTime = -1;
					pin0shutterOpenEndTime = -1;
					pin1shutterOpenStartTime = -1;
					pin1shutterOpenEndTime = -1;
					pin2shutterOpenStartTime = -1;
					pin2shutterOpenEndTime = -1;
					sensor0Max = 0;
					sensor1Max = 0;
					sensor2Max = 0;
				}
				break;
			}
			// case AdcISRFlow::FAST_MEASURED:
			// {
			// 	adcISRFlow = AdcISRFlow::NONE;
			// 	appModeChanged = false;
			// 	setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
			// 	pin1shutterOpenStartTime = -1;
			// 	pin1shutterOpenEndTime = -1;
			// 	sensor1Max = 0;
			// 	// pin1StartTimeSet = false;
			// 	// pin1EndTimeSet = false;
			// 	setADCautoTriggerEnabled(false);
			// 	startADCconversion();
			// 	break;
			// }
			case AdcISRFlow::SENSOR_READINGS_CHECK:
			{
				disableADCinterrupt();
				adcISRFlow = AdcISRFlow::PWM_LIGHT_CHECK;
				appModeChanged = false;
				break;
			}
			case AdcISRFlow::PWM_LIGHT_CHECK:
			{
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_128);
				setADCautoTriggerEnabled(false);
				delay(50);
				adcISRFlow = AdcISRFlow::NONE;
				appModeChanged = false;
				startADCconversion();
				break;
			}
			default:
				break;
		}
	}

	if (button.isHolded())
	{
		switch (adcISRFlow)
		{
			case AdcISRFlow::NONE:
			{
				adcISRFlow = AdcISRFlow::SENSOR_READINGS_CHECK;
				setADCprescaler(ADCPrescaler::ADC_PRESCALER_4);
				// appModeChanged = false;
				break;
			}
			// case AdcISRFlow::MEASURING:
			// {
			// 	adcISRFlow = AdcISRFlow::FAST_MEASURING;
			// 	delay(50);// wait a bit so ADC interrupt will switch to new mode and it won't change new ADC pin setting
			// 	setADCInputPin(2);
			// 	appModeChanged = false;
			// 	break;
			// }
			default:
				break;
		}
	}

	if (adcISRFlow == AdcISRFlow::MEASURING &&
		 ((pin0shutterOpenStartTime != -1 && pin0shutterOpenEndTime != -1) ||
		 (pin1shutterOpenStartTime != -1 && pin1shutterOpenEndTime != -1) ||
		 (pin2shutterOpenStartTime != -1 && pin2shutterOpenEndTime != -1)))//Check if at least one sensor has data
	{
		delay(500);//wait for other sensors to get data
		adcISRFlow = AdcISRFlow::MEASURED;
		appModeChanged = false;
		pinResultIndex = 0;
	}
}