
#include <Arduino.h>
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "lib/Common.h"
#include "lib/AlexEncoder.h"
#include "lib/Display.h"
#include "lib/ADC.h"
#include "lib/SerialNumber.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "lib/serialAPI.h"

#include "screens/MainMenu.h"
#include "screens/SelectItemScreen.h"



void setup() 
{
	// pinMode(15, OUTPUT);
	// digitalWrite(15, HIGH);
	// delay(2000);
	// halt();

	pinMode(TEST_PIN_1, OUTPUT);
	pinMode(TEST_PIN_2, OUTPUT);

	// for (int i = 0; i < 3; i++)
	// {
	// 	digitalWrite(15, HIGH);
	// 	delay(500);
	// 	digitalWrite(15, LOW);
	// 	delay(500);
	// }

	// dacWrite(DAC1, 0);
	// delay(1000000000);

	// esp_log_level_set("*", ESP_LOG_NONE);

	display->begin();
  display->fillScreen(BLACK);
  display->setTextColor(WHITE);
  display->setFont(u8g2_font_6x13_tf);
	
	// while (true)
	// {
	// 	for (size_t y = 0; y < 50; y+=5)
	// 	{
	// 		display->drawRect(y, y, 10, 10, GREEN);
	// 	}
	// }

	// 	display->setCursor(10, 30);
	// 	display->print("Display test");

	// 	// delay(1000);
	// 	// display->fillScreen(RED);
	// 	// delay(1000);
	// 	// display->fillScreen(GREEN);
	// 	// delay(1000);
	// 	// display->fillScreen(BLUE);
	// 	// delay(1000);
	// 	// display->fillScreen(WHITE);
	// 	// delay(1000);
	// }

	AlexEncoder::init(ENCODER_A_PIN, ENCODER_B_PIN);

  auto timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, US_IN_SECOND / USER_INPUT_POLLING_FREQ_HZ, true, 0);

  initADC();
	initStorage();

	storage.begin();
	seriesStorage.begin();

	// Backfill names for new-format files that still have empty names
	// (e.g. records written through the serial API which we do not touch).
	fillEmptyRecordNames<RecordsStorageManager, StoredMeasuredResult>(storage);
	fillEmptyRecordNames<SeriesStorageManager, StoredMeasuredSeries>(seriesStorage);

	SerialNumber::init();

	// Инициализируем мьютекс для безопасного вывода в последовательный порт
	serialPrintMutex = xSemaphoreCreateMutex();

	xTaskCreatePinnedToCore(serialApiTask, "SerialAPI", 8 * 1024, NULL, 1, NULL, 0);

	esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
	// esp_ota_mark_app_invalid_rollback_and_reboot();//mark as invalid in case of system test if failed

	if (err == ESP_OK)
	{
		ESP_LOGI("", "Firmware validated, rollback cancelled.");
	}
	else
	{
		ESP_LOGE("", "Error validating firmware: %s", esp_err_to_name(err));
	}
}

// // ---- Mock data for testing the series report without real measurements ----
// // Simulates 5 shots of a 35mm camera with horizontal focal-plane shutter.
// // Values are computed the same way computeMeasuredResult() does:
// // sensor distance 16 mm, frame width 36 mm (see sensorsData[Frame35mm]).
// // The mock covers the whole value spectrum: every standard shutter speed from
// // 1/4000 s (250 us) to 30 s combined with every curtain speed from the
// // 0.5 .. 10 m/s grid (18 x 6 = 108 sets). Each shot deviates from its nominal
// // values by up to +/- MOCK_VARIATION_PERCENT percent, like shots of a real
// // series differ from each other. NOTE: extreme combinations (e.g. 30 s at
// // 0.5 m/s) are physically impossible (huge slit width) - they are kept on
// // purpose to exercise the extreme-value formatting.

// #define MOCK_SENSOR_DISTANCE_MM 16.0
// #define MOCK_FRAME_WIDTH_MM 36.0

// // Spread applied to the raw mock inputs: 20 => each input is multiplied by a
// // factor uniformly distributed in [0.8 .. 1.2]
// #define MOCK_VARIATION_PERCENT 70

// // Deterministic PRNG (LCG, Numerical Recipes constants) so the mock spread
// // is identical after every boot. Replace with random() if per-boot randomness
// // is preferred.
// static uint32_t mockRandomState = 0x1234ABCD;

// double nextMockVariationFactor()
// {
// 	mockRandomState = mockRandomState * 1664525u + 1013904223u;

// 	const double unit = (double)(mockRandomState >> 8) / 16777216.0; // 0..1 from 24 bits
// 	const double spread = MOCK_VARIATION_PERCENT * 2.0 / 100.0;      // 0.4 for +/-20%
// 	return 1.0 - spread / 2.0 + unit * spread;                       // 0.8 .. 1.2
// }

// // Standard shutter speeds from 1/4000 s to 30 s (full-stop row), in milliseconds
// const double mockShutterSpeedsMs[] =
// 	{
// 		0.25,          // 1/4000
// 		0.5,           // 1/2000
// 		1.0,           // 1/1000
// 		2.0,           // 1/500
// 		4.0,           // 1/250
// 		8.0,           // 1/125
// 		1000.0 / 60.0, // 1/60
// 		1000.0 / 30.0, // 1/30
// 		1000.0 / 15.0, // 1/15
// 		125.0,         // 1/8
// 		250.0,         // 1/4
// 		500.0,         // 1/2
// 		1000.0,        // 1"
// 		2000.0,        // 2"
// 		4000.0,        // 4"
// 		8000.0,        // 8"
// 		15000.0,       // 15"
// 		30000.0,       // 30"
// };

// // Curtain speeds grid, in meters per second (0.5 + step 2, 10 added as the bound)
// const double mockCurtainSpeedsMS[] = {0.5, 2.5, 4.5, 6.5, 8.5, 10.0};

// const uint8_t mockShutterSpeedsCount = sizeof(mockShutterSpeedsMs) / sizeof(mockShutterSpeedsMs[0]);
// const uint8_t mockCurtainSpeedsCount = sizeof(mockCurtainSpeedsMS) / sizeof(mockCurtainSpeedsMS[0]);

// MeasuredResult createMockMeasuredResult(double sensor0TimeMs, double sensor1TimeMs, double curtain1SpanTimeMs, double curtain2SpanTimeMs)
// {
// 	MeasuredResult r = {};

// 	r.sensor0Time = sensor0TimeMs;
// 	r.sensor1Time = sensor1TimeMs;

// 	r.curtain1spanAtime = curtain1SpanTimeMs;
// 	r.curtain1spanAspeed = MOCK_SENSOR_DISTANCE_MM / curtain1SpanTimeMs;       // m/s
// 	r.curtain1TotalTime = MOCK_FRAME_WIDTH_MM / r.curtain1spanAspeed;          // ms

// 	r.curtain2spanAtime = curtain2SpanTimeMs;
// 	r.curtain2spanAspeed = MOCK_SENSOR_DISTANCE_MM / curtain2SpanTimeMs;       // m/s
// 	r.curtain2TotalTime = MOCK_FRAME_WIDTH_MM / r.curtain2spanAspeed;          // ms

// 	double estSlitSpeed = MOCK_SENSOR_DISTANCE_MM / ((curtain1SpanTimeMs + curtain2SpanTimeMs) / 2.0); // mm per ms
// 	r.slitWidthSensor0 = estSlitSpeed * sensor0TimeMs;                         // mm
// 	r.slitWidthSensor1 = estSlitSpeed * sensor1TimeMs;                         // mm
// 	r.slitWidthAverage = (r.slitWidthSensor0 + r.slitWidthSensor1) / 2.0;      // mm

// 	r.usedSensorType = SensorType::Frame35mm;
// 	r.selectedCurtainMovement = CurtainMovement::HORISONTAL;

// 	return r;
// }

// // Builds a series of 5 shots around the given nominal shutter speed and
// // curtain speed. Curtain speed is not stored directly - the curtain span time
// // is derived from it (spanTime = MOCK_SENSOR_DISTANCE_MM / speed), so the
// // computeMeasuredResult-style math reproduces the requested speed. Every raw
// // input of every shot gets an independent +/-20% deviation from its nominal
// // value; speeds, total times and slit widths are derived from those inputs
// // inside createMockMeasuredResult() and inherit the spread naturally.
// MeasurementSeries createMockMeasurementSeries(double nominalShutterSpeedMs, double nominalCurtainSpeedMS)
// {
// 	MeasurementSeries series = {};
// 	series.totalCount = 5;
// 	series.storedCount = 5;

// 	const double nominalCurtainSpanTimeMs = MOCK_SENSOR_DISTANCE_MM / nominalCurtainSpeedMS; // mm/(mm/ms) = ms

// 	for (uint8_t i = 0; i < series.storedCount; i++)
// 	{
// 		const double sensor0TimeMs = nominalShutterSpeedMs * nextMockVariationFactor();
// 		const double sensor1TimeMs = nominalShutterSpeedMs * nextMockVariationFactor();
// 		const double curtain1SpanTimeMs = nominalCurtainSpanTimeMs * nextMockVariationFactor();
// 		const double curtain2SpanTimeMs = nominalCurtainSpanTimeMs * nextMockVariationFactor();

// 		series.results[i] = createMockMeasuredResult(sensor0TimeMs, sensor1TimeMs, curtain1SpanTimeMs, curtain2SpanTimeMs);
// 	}

// 	return series;
// }

// ---- Temporary mock data for testing drawSelectItemScreen() ----
// loop() calls the new screen with this list instead of drawMainMenu() until
// the screen is verified on hardware. The list mixes: short one-line names, a
// name of exactly 23 chars (the one-line limit), real two-line lengths
// (24..28, as in FilmCamerasList), a long name that wraps at a space and an
// oversized one that is clipped to two lines. The last page stays partially
// filled on purpose.
// static const char *mockSelectItemNames[] =
// 		{
// 				"Nikon F",																					// 7 -> one line
// 				"Hasselblad 500C/M with 80mm Planar lens 1",					// 39 -> two lines, wraps at a space
// 				"Hasselblad 500C/M with 80mm Planar lens 2",					// 39 -> two lines, wraps at a space
// 				"Hasselblad 500C/M with 80mm Planar lens 3",					// 39 -> two lines, wraps at a space
// 				"Hasselblad 500C/M with 80mm Planar lens 4",					// 39 -> two lines, wraps at a space
// 				"Hasselblad 500C/M with 80mm Planar lens 5",					// 39 -> two lines, wraps at a space
// 				"Hasselblad 500C/M with 80mm Planar lens 6",					// 39 -> two lines, wraps at a space
// 				"Canon AE-1",																				// 10 -> one line
// 				"Mamiya C330 Professional",													// 24 -> two lines
// 				"Leica M3",																					// 8 -> one line
// 				"Fujifilm GA645 Professional",											// 27 -> two lines
// 				"Pentax K1000",																			// 12 -> one line
// 				"Canon Canonet QL17 GIII",													// 23 -> exactly one line
// 				"Zenza Bronica S2A",																// 17 -> one line
// 				"Yashica Mat-124G",																	// 16 -> one line
// 				"Rolleiflex 3.5F TLR",															// 19 -> one line
// 				"Olympus OM-1n",																		// 13 -> one line
// 				"Contax T2",																				// 9 -> one line
// 				"Kiev 88",																					// 7 -> one line
// 				"Praktica LTL 3",																		// 14 -> one line
// 				"Mamiya RB67 Professional SD with 90mm f/3.8 lens", // 48 -> clipped to two lines
// 				"Nikon FM2n",																				// 10 -> one line
// 				"Minolta X-700",																		// 13 -> one line
// 				"Chinon CE-4s",																			// 12 -> one line
// 				"Fujica ST801",																			// 12 -> one line
// 				"Konica Autoreflex T3",															// 20 -> one line
// 				"Hasselblad 503CW",																	// 16 -> one line
// 				"Voigtlander Bessa R2",															// 20 -> one line
// 				"Mamiya 7II with 80mm lens"													// 25 -> two lines
// };

// static const int16_t mockSelectItemNamesCount = sizeof(mockSelectItemNames) / sizeof(mockSelectItemNames[0]);

void loop() 
{
	/* MeasuredResult r = {
			3,
			4,
			3,
			4,
			5,
			6,
			7,
			8,
			9,
			12,
			12};
	drawMeasurementSaveScreen(r); */

	/* sensor0Max = 2000;
	sensor1Max = 2100;
	curtainMovement = CurtainMovement::HORISONTAL;
	drawMeasuredScreen(490,495, 2500, 2510); */

	// drawViewRecordsScreen();

	// halt();

	// while (true)
	// {
	// 	curSensorIndex = 0;
	// 	curtainMovement = CurtainMovement::VERTICAL;
	// 	drawMeasuringScreen();
	// }

	// while (true)
	// {
	// 	if (button.isClicked())
	// 	{
	// 		curSensorIndex = 0;
	// 		curtainMovement = CurtainMovement::VERTICAL;
	// 		drawMeasuringScreen();
	// 	}

	// 	vTaskDelay(1 / portTICK_PERIOD_MS);
	// }

	// // Cycle through the whole mock grid (standard shutter speeds x curtain
	// // speeds): every exit from the series report shows the next combination,
	// // wrapping around after the last one. drawMainMenu() is disabled here on
	// // purpose - it never returns, so it would stop the sweep after one set.
	// static uint8_t mockComboIndex = 0;

	// while (true)
	// {
	// 	const uint8_t shutterSpeedIndex = mockComboIndex / mockCurtainSpeedsCount;
	// 	const uint8_t curtainSpeedIndex = mockComboIndex % mockCurtainSpeedsCount;

	// 	MeasurementSeries measurementSeries = createMockMeasurementSeries(mockShutterSpeedsMs[shutterSpeedIndex], mockCurtainSpeedsMS[curtainSpeedIndex]);

	// 	renderSeriesReport(measurementSeries);

	// 	mockComboIndex = (mockComboIndex + 1) % (mockShutterSpeedsCount * mockCurtainSpeedsCount);
	// }

	// // ---- Temporary: exercise drawSelectItemScreen() with mock data instead
	// // ---- of the main menu (restore drawMainMenu() after the hardware test).
	// int16_t selectedItemIndex = drawSelectItemScreen("Select camera", mockSelectItemNames, mockSelectItemNamesCount);

	// // Show what the screen returned ("Go Back" gives -1) and open the list
	// // again, so both outcomes can be checked one after another.
	// display->setFont(u8g2_font_6x13_tf);
	// display->setTextColor(WHITE);
	// char selectedItemTitle[24];
	// snprintf(selectedItemTitle, sizeof(selectedItemTitle), "Selected: %d", selectedItemIndex);
	// static const char *mockResultOptions[] = {"Ok"};
	// drawMessageScreen(selectedItemTitle, 1, mockResultOptions);

	drawMainMenu();
}