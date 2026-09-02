
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

	drawMainMenu();
}