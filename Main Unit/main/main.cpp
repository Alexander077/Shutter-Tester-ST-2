
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

// ---- Mock data for testing the series report without real measurements ----
// Simulates 5 shots of a 35mm camera with horizontal focal-plane shutter.
// Values are computed the same way computeMeasuredResult() does:
// sensor distance 16 mm, frame width 36 mm (see sensorsData[Frame35mm]).

#define MOCK_SENSOR_DISTANCE_MM 16.0
#define MOCK_FRAME_WIDTH_MM 36.0

MeasuredResult createMockMeasuredResult(double sensor0TimeMs, double sensor1TimeMs, double curtain1SpanTimeMs, double curtain2SpanTimeMs)
{
	MeasuredResult r = {};

	r.sensor0Time = sensor0TimeMs;
	r.sensor1Time = sensor1TimeMs;

	r.curtain1spanAtime = curtain1SpanTimeMs;
	r.curtain1spanAspeed = MOCK_SENSOR_DISTANCE_MM / curtain1SpanTimeMs;       // m/s
	r.curtain1TotalTime = MOCK_FRAME_WIDTH_MM / r.curtain1spanAspeed;          // ms

	r.curtain2spanAtime = curtain2SpanTimeMs;
	r.curtain2spanAspeed = MOCK_SENSOR_DISTANCE_MM / curtain2SpanTimeMs;       // m/s
	r.curtain2TotalTime = MOCK_FRAME_WIDTH_MM / r.curtain2spanAspeed;          // ms

	double estSlitSpeed = MOCK_SENSOR_DISTANCE_MM / ((curtain1SpanTimeMs + curtain2SpanTimeMs) / 2.0); // mm per ms
	r.slitWidthSensor0 = estSlitSpeed * sensor0TimeMs;                         // mm
	r.slitWidthSensor1 = estSlitSpeed * sensor1TimeMs;                         // mm
	r.slitWidthAverage = (r.slitWidthSensor0 + r.slitWidthSensor1) / 2.0;      // mm

	r.usedSensorType = SensorType::Frame35mm;
	r.selectedCurtainMovement = CurtainMovement::HORISONTAL;

	return r;
}

MeasurementSeries createMockMeasurementSeries()
{
	MeasurementSeries series = {};
	series.totalCount = 5;
	series.storedCount = 5;

	// Slightly different values from shot to shot, like in a real series
	const double sensor0Times[] = {0.256, 0.240, 0.276, 0.264, 0.250};       // ms
	const double sensor1Times[] = {0.288, 0.248, 0.292, 0.244, 0.254};       // ms
	const double curtain1SpanTimes[] = {2.77, 2.98, 3.01, 2.68, 3.03};  // ms
	const double curtain2SpanTimes[] = {2.87, 2.98, 2.7, 2.78, 2.99};  // ms

	for (uint8_t i = 0; i < series.storedCount; i++)
	{
		series.results[i] = createMockMeasuredResult(sensor0Times[i], sensor1Times[i], curtain1SpanTimes[i], curtain2SpanTimes[i]);
	}

	return series;
}

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

	MeasurementSeries measurementSeries = createMockMeasurementSeries();

	renderSeriesReport(measurementSeries);

	drawMainMenu();
}