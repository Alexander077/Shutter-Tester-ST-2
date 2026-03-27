
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include <mString.h>
#include <InterpolationLib.h>
#include "lib/AceSorting/src/AceSorting.h"
#include "esp_littlefs.h"

#include "lib/AlexEncoder.h"
#include "lib/AlexButton.h"
#include "lib/Common.h"
#include "lib/DisplayManager.h"
#include "lib/StoredMeasuredResult.h"
#include "lib/About.h"
#include "lib/Images.h"
#include "lib/SerialAPI.h"
#include "lib/RecordsStorageManager.h"

#include "esp_partition.h"

// #define DISPLAY_CS SS
#define DISPLAY_RESET 18
#define DISPLAY_DC 33

#define ENCODER_B_PIN 12
#define ENCODER_A_PIN 13
#define BUTTON_PIN 14

#define TEST_PIN 5

#define USER_INPUT_POLLING_FREQ_HZ 300
// #define SPLASH_SCREEN_VISIBLE_TIME_MS 1000000000
#define SPLASH_SCREEN_VISIBLE_TIME_MS 1250

#define MM_IN_M 1000
#define US_IN_SECOND 1000000
#define US_IN_MINUTE 60000000
#define US_IN_MILLISECOND 1000

#define ONE_ADC_CONVERSION_TIME_US 24.7746

#define TOTAL_RECORDS_COUNT 100

#define LITTLEFS_PARTITION_LABEL "storage"
#define LITTLEFS_PARTITION_NAME "littlefs"
#define LITTLEFS_BASE_PATH "/" LITTLEFS_PARTITION_NAME
#define RECORDS_FILE_NAME "records.bin"
#define RECORDS_FILE_PATH LITTLEFS_BASE_PATH "/" RECORDS_FILE_NAME

RecordsStorageManager storage(RECORDS_FILE_PATH);

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

enum class LightQualityStatus
{
  UNKNOWN,
  OK,
  BAD,
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

enum class CurtainMovementSelectionScreenResult
{
	GO_BACK,
	GO_TO_MAIN_MENU
};

DisplayManager displayManager;

uint16_t sensor0Max = 0;
uint16_t sensor1Max = 0;

#define INTRPOLATION_POINTS_COUNT 5
double sensorMaxAdcVals[INTRPOLATION_POINTS_COUNT] = {1300, 2000, 2700, 3300, 4000};
double timeCorrectionVals[INTRPOLATION_POINTS_COUNT] = {40, 40, 30, 20, 15};

#define SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX 5
#define SPEEDS_GRAPH_BAR_WIDTH_PX 146

#define SHUTTR_SPEEDS_COUNT 14
const uint16_t shutterSpeeds[] = {8000, 4000, 2000, 1000, 500, 250, 125, 60, 30, 15, 8, 4, 2, 1};

#define ADC_UNIT ADC_UNIT_1
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_BIT_WIDTH SOC_ADC_DIGI_MAX_BITWIDTH
#define ADC_READ_LEN 256
#define ADC_CONVERSION_FREQ_HZ 80000

#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define ADC_GET_CHANNEL(p_data) ((p_data)->type1.channel)
#define ADC_GET_DATA(p_data) ((p_data)->type1.data)
static adc_channel_t channel[2] = {ADC_CHANNEL_1, ADC_CHANNEL_2};

// static TaskHandle_t s_task_handle;
adc_continuous_handle_t handle = NULL;

void startFirmwareUpdate();

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
  BaseType_t mustYield = pdFALSE;
  // Notify that ADC continuous driver has done enough number of conversions
  // vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

  return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
  adc_continuous_handle_t handle = NULL;

  adc_continuous_handle_cfg_t adc_config = {
      .max_store_buf_size = 1024,
      .conv_frame_size = ADC_READ_LEN,
  };
  ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

	adc_continuous_config_t dig_cfg = {
			.sample_freq_hz = ADC_CONVERSION_FREQ_HZ,
			// .sample_freq_hz = SOC_ADC_SAMPLE_FREQ_THRES_HIGH,
			.conv_mode = ADC_CONV_MODE,
			.format = ADC_OUTPUT_TYPE,
	};

	adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
  dig_cfg.pattern_num = channel_num;

  for (int i = 0; i < channel_num; i++)
  {
    adc_pattern[i].atten = ADC_ATTEN;
    adc_pattern[i].channel = channel[i] & 0x7;
    adc_pattern[i].unit = ADC_UNIT;
    adc_pattern[i].bit_width = ADC_BIT_WIDTH;

    ESP_LOGI("", "adc_pattern[%d].atten is :%" PRIx8, i, adc_pattern[i].atten);
    ESP_LOGI("", "adc_pattern[%d].channel is :%" PRIx8, i, adc_pattern[i].channel);
    ESP_LOGI("", "adc_pattern[%d].unit is :%" PRIx8, i, adc_pattern[i].unit);
  }
  
  dig_cfg.adc_pattern = adc_pattern;
  ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

  *out_handle = handle;
}

struct SensorUnitData
{
  SensorType Type;
  double FrameWidth;               // in millimiters
  double FrameHeight;              // in millimiters
  double HorisontalSensorDistance; // in millimiters
  double VerticalSensorDistance;   // in millimiters
};

const SensorUnitData sensorsData[sensorsDataArraySize] = 
{
	{
		SensorType::Frame35mm,
		36,
		24,
		16.0,
		10.6,
	},
	{
		SensorType::Frame6x45,
		60,
		45,
		16.0,
		10.6,
	},
	{
		SensorType::Frame6x6,
		60,
		60,
		16.0,
		10.6,
	},
	{
		SensorType::Frame6x7,
		70,
		60,
		16.0,
		10.6,
	}
};

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
	AlexEncoder::tick();
}

int16_t getMeasurementSaveRecordSize()
{
  return sizeof(StoredMeasuredResult);
}

int16_t getTotalMeasurementSaveSlotsCount()
{
	// return EEPROM_MEASURED_RES_TOTAL_BYTES / getMeasurementSaveRecordSize();
	return TOTAL_RECORDS_COUNT;
}

int16_t getFreeMeasurementSaveSlotsCount()
{
	return storage.getFreeSlotsCount();
}

StoredMeasuredResult getMeasurementStoredResultByNumber(int32_t recordNumber)
{
	return storage.getRecordByNumber(recordNumber);
}

bool deleteMeasurementStoredResultByNumber(int32_t recordNumber)
{
	return storage.deleteRecordByNumber(recordNumber);
}

int32_t drawMeasurementResultRecordSelectionScreen()
{
	// 1. Получаем все существующие (не удаленные) номера записей через менеджер
	std::vector<int32_t> validIds = storage.getAllValidRecordNumbers();

	// 2. Размер массива = количество записей + 1 (для кнопки "Back")
	int16_t totalArraySaize = validIds.size() + 1;
	int32_t recordNumbers[totalArraySaize];

	recordNumbers[0] = 0; // Опция "Back" (возврат)

	// 3. Заполняем массив данными из вектора
	for (size_t i = 0; i < validIds.size(); i++)
	{
		recordNumbers[i + 1] = validIds[i];
	}

	// Сортировка (оставляем вашу оригинальную логику)
	ace_sorting::shellSortKnuth(recordNumbers, totalArraySaize);

	int16_t startEncoderVal = AlexEncoder::counter;
	display->fillScreen(BLACK);

	uint8_t xMargin = 30;
	uint8_t yMargin = 40;
	uint8_t ySpacing = 15;
	uint8_t arrowXmargin = 15;
	uint8_t curPageLabelY = 120;
	mString<30> pageLabel;
	const int16_t pageSize = 5;
	int16_t prevPage = 1;
	int16_t curPage = 1;
	const int16_t recTitleBufSize = 30;
	int16_t totalPagesCount = ceil((double)totalArraySaize / (double)pageSize);
	drawStringHCentered("Select record", 20);
	int8_t itemsCountToPrint = min(pageSize, totalArraySaize);

	for (uint8_t i = 0; i < itemsCountToPrint; i++)
	{
		display->setCursor(xMargin, yMargin + (ySpacing * i));

		// int32_t recordNumber = getInputParamsArrayInt(i + 3);
		int32_t recordNumber = recordNumbers[i];

		if (recordNumber == 0) //"Back" option
		{
			display->print("Back");
			continue;
		}

		char recTitle[recTitleBufSize];
		formatRecordName(recordNumber, recTitle);
		display->print(recTitle);

		pageLabel.clear();
		pageLabel += "Page ";
		pageLabel.add(curPage);
		pageLabel += " of ";
		pageLabel.add(totalPagesCount);
		drawStringHCentered(pageLabel, curPageLabelY);
	}

	display->setCursor(xMargin - arrowXmargin, yMargin);
	display->print("->");

	int8_t prevSelectedRecordIndex = 0;
	int8_t topRecordIndex = 0;
	int8_t prevTopRecordIndex = 0;

	while (true)
	{
		if (isApiRequestReceived)
			return 0; // ВЫХОД ДЛЯ API

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

		// ----------------------------
    // displayManager.drawMeasurementResultRecordSelectionScreen(curIndex, recordNumbers, totalArraySaize);

		if (prevSelectedRecordIndex != curIndex)
		{
			display->setTextColor(BLACK);
			display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * (prevSelectedRecordIndex % pageSize)));
			display->print("->");

			display->setTextColor(WHITE);
			display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * (curIndex % pageSize)));
			display->print("->");

			prevSelectedRecordIndex = curIndex;

			curPage = ceil((curIndex + 1) / (double)pageSize);
			// ESP_LOGI("", "Cur. page: %" PRIi16, curPage);

			if (prevPage != curPage)
			{
				// ------------------ erase cur page
				display->setTextColor(BLACK);

				int16_t prevPageMaxIndex = pageSize * (prevPage - 1) + pageSize;
				int16_t prevPageLimit = min(prevPageMaxIndex, totalArraySaize);

				for (uint8_t i = pageSize * (prevPage - 1), rowCounter = 0; i < prevPageLimit; i++, rowCounter++)
				{
					display->setCursor(xMargin, yMargin + (ySpacing * rowCounter));
					// int32_t recordNumber = getInputParamsArrayInt(i + 3);
					int32_t recordNumber = recordNumbers[i];
					char recTitle[recTitleBufSize];
					formatRecordName(recordNumber, recTitle);

					if (recordNumber == 0) //"Back" option
					{
						display->print("Back");
						continue;
					}

					display->print(recTitle);
				}

				pageLabel.clear();
				pageLabel += "Page ";
				pageLabel.add(prevPage);
				pageLabel += " of ";
				pageLabel.add(totalPagesCount);
				drawStringHCentered(pageLabel, curPageLabelY);
				//--------------------------- end of erase cur page

				display->setTextColor(WHITE);
				int16_t nextPageMaxIndex = pageSize * (curPage - 1) + pageSize;
				int16_t limit = min(nextPageMaxIndex, totalArraySaize);

				// draw new page records
				for (int16_t i = pageSize * (curPage - 1), rowCounter = 0; i < limit; i++, rowCounter++)
				{
					display->setCursor(xMargin, yMargin + (ySpacing * rowCounter));
					// int32_t recordNumber = getInputParamsArrayInt(i + 3);
					int32_t recordNumber = recordNumbers[i];

					if (recordNumber == 0) //"Back" option
					{
						display->print("Back");
						continue;
					}

					char recTitle[recTitleBufSize];
					formatRecordName(recordNumber, recTitle);
					// ESP_LOGI("", "%" PRIi32, recordNumber);
					display->print(recTitle);
				}

				pageLabel.clear();
				pageLabel += "Page ";
				pageLabel.add(curPage);
				pageLabel += " of ";
				pageLabel.add(totalPagesCount);
				drawStringHCentered(pageLabel, curPageLabelY);

				prevPage = curPage;
			}
		}

		// ----------------------------

		if (button.isClicked())
		{
			return recordNumbers[curIndex];
		}
	}
}

MeasurementRecordSaveResult saveMesurementResult(const MeasuredResult &res, bool overwriteOldest = false, bool overwriteNewest = false, int32_t recordNumberToOverwrite = -1)
{
	MeasurementRecordSaveResult saveRes = {false, -1};

	// Создаем структуру для сохранения и зануляем её память
	StoredMeasuredResult newRes;
	memset(&newRes, 0, sizeof(StoredMeasuredResult));

	// Если передали recordNumberToOverwrite, значит это режим перезаписи (update)
	// Иначе ставим 0, что для менеджера означает "создать новую запись или найти свободный слот"
	if (recordNumberToOverwrite != -1)
	{
		newRes.recordNumber = recordNumberToOverwrite;
	}
	else
	{
		newRes.recordNumber = 0;
	}

	newRes.isDeleted = false;

	// Аккуратно перекладываем все данные из текущего измерения
	newRes.sensor0Time = res.sensor0Time;
	newRes.sensor1Time = res.sensor1Time;
	newRes.curtain1spanAspeed = res.curtain1spanAspeed;
	newRes.curtain1spanAtime = res.curtain1spanAtime;
	newRes.curtain1TotalTime = res.curtain1TotalTime;
	newRes.curtain2spanAspeed = res.curtain2spanAspeed;
	newRes.curtain2spanAtime = res.curtain2spanAtime;
	newRes.curtain2TotalTime = res.curtain2TotalTime;
	newRes.slitWidthSensor0 = res.slitWidthSensor0;
	newRes.slitWidthSensor1 = res.slitWidthSensor1;
	newRes.slitWidthAverage = res.slitWidthAverage;
	newRes.usedSensorType = res.usedSensorType;
	newRes.selectedCurtainMovement = res.selectedCurtainMovement;

	// --- Обработка команд перезаписи самых новых или самых старых результатов ---
	if (overwriteOldest || overwriteNewest)
	{
		std::vector<int32_t> ids = storage.getAllValidRecordNumbers();

		if (!ids.empty())
		{
			// Находим минимальный (самый старый) и максимальный (самый новый) ID
			int32_t minId = ids[0];
			int32_t maxId = ids[0];

			for (int32_t id : ids)
			{
				if (id < minId)
					minId = id;
				if (id > maxId)
					maxId = id;
			}

			if (overwriteOldest)
			{
				newRes.recordNumber = minId;
			}
			else if (overwriteNewest)
			{
				newRes.recordNumber = maxId;
			}
		}
		else
		{
			// Если записей нет, но запрошена перезапись — просто сохраняем как новую
			newRes.recordNumber = 0;
		}
	}

	// Делегируем всю работу с файлом LittleFS и мьютексами классу-менеджеру
	int32_t savedId = storage.saveOrUpdateRecord(newRes);

	// Обработка результата
	if (savedId != -1)
	{
		saveRes.isSuccess = true;
		saveRes.newRecordNumber = savedId;
		ESP_LOGI("", "Meas. res. saved. Rec. num.: %" PRIi32 ", success: 1", savedId);
	}
	else
	{
		ESP_LOGE("", "Failed to save measurement result");
	}

	return saveRes;
}

double getCorrectedSensorValue(long rawSensorTime, uint16_t maxSensorValue)
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

int16_t drawMessageScreen(const char *title, int16_t optionsCount = 0, const char *options[] = {})
{
  int16_t startEncoderVal = AlexEncoder::counter;
	int8_t prevSelectedMenuItemIndex = 0;
	uint8_t xMargin = 35;
	uint8_t yMargin = 50;
	uint8_t ySpacing = 15;
	uint8_t arrowXmargin = 15;

	display->fillScreen(BLACK);
	drawStringHCentered(title, 25);

	for (int16_t i = 0; i < optionsCount; i++)
	{
		display->setCursor(xMargin, yMargin + (ySpacing * i));
		display->print(options[i]);
	}

	display->setCursor(xMargin - arrowXmargin, yMargin);
	display->print("->");

	while (true)
	{
		// === ВЫХОД ДЛЯ API ===
		if (isApiRequestReceived)
			return 0;

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

		// -------------------------------
    // displayManager.drawMessageScreen(messageScreenId, useProgMem, title, options, optionsCount, resultOptionIndex);

		if (prevSelectedMenuItemIndex != resultOptionIndex)
		{
			display->setTextColor(BLACK);
			display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
			display->print("->");

			display->setTextColor(WHITE);
			display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultOptionIndex));
			display->print("->");

			prevSelectedMenuItemIndex = resultOptionIndex;
		}
		// -------------------------------

    if (button.isClicked())
    {
      return resultOptionIndex;
    }
  }
}

MeasurementSaveScreenResult drawMeasurementSaveScreen(MeasuredResult &measurementRes)
{
	int16_t freeSlotsCount = getFreeMeasurementSaveSlotsCount();
	int16_t totalRecordsCount = getTotalMeasurementSaveSlotsCount();
  int16_t menuItemsCount = totalRecordsCount == freeSlotsCount ? SAVE_MEASUREMENT_MENU_ZERO_RECORDS_ITEMS_COUNT : SAVE_MEASUREMENT_MENU_ITEMS_COUNT;
  int16_t startEncoderVal = AlexEncoder::counter;
  bool drawMoreThanZeroRecordsMenuItems = menuItemsCount == SAVE_MEASUREMENT_MENU_ITEMS_COUNT;

	uint8_t xMargin = 20;
	uint8_t yMargin = 40;
	uint8_t ySpacing = 15;
	uint8_t arrowXmargin = 15;

	while (true)
	{
		display->fillScreen(BLACK);
		drawStringHCentered("Save measurement result?", 20);
		
		for (uint8_t i = 0; i < menuItemsCount; i++)
		{
			display->setCursor(xMargin, yMargin + (ySpacing * i));

			if (i == (uint8_t)MeasurementSaveMenuItems::YES)
			{
				if (freeSlotsCount == 0)
				{
					display->setTextColor(DARKGREY);
					display->printf("%s(no free slots)", GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
					display->setTextColor(WHITE);
				}
				else
				{
					display->printf("%s(%i free slots)", GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems), freeSlotsCount);
				}
			}
			else
			{
				display->println(GetSaveMasurementMenuItemTitle((MeasurementSaveMenuItems)i, !drawMoreThanZeroRecordsMenuItems));
			}
		}

		display->setCursor(xMargin - arrowXmargin, yMargin);
		display->print("->");

		int16_t prevSelectedMenuItemIndex = 0;
		bool runLoop = true;

		while (runLoop)
		{
			// === ВЫХОД ДЛЯ API ===
			if (isApiRequestReceived)
				return MeasurementSaveScreenResult::CANCEL;

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

			//-------------------------
			// displayManager.drawMeasurementSaveScreen(resultIndex, freeSlotsCount, drawMoreThanZeroRecordsMenuItems);

			if (prevSelectedMenuItemIndex != resultIndex)
			{
				display->setTextColor(BLACK);
				display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
				display->print("->");

				display->setTextColor(WHITE);
				display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultIndex));
				display->print("->");

				prevSelectedMenuItemIndex = resultIndex;
			}

			//-----------------------------

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
								ESP_LOGI("","Saving record...");
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
							ESP_LOGI("","Saving record...");
							saveRes = saveMesurementResult(measurementRes, false, true);
							break;
						}
						case MeasurementSaveMenuItems::OVERWRITE_OLDEST:
						{
							ESP_LOGI("","Saving record...");
							saveRes = saveMesurementResult(measurementRes, true);
							break;
						}
						case MeasurementSaveMenuItems::CHOOSE_RECORD_TO_OVERWRITE:
						{
							int32_t selectedMesurementRecordNumber = drawMeasurementResultRecordSelectionScreen();

							if (selectedMesurementRecordNumber == 0) // user selected "Back" option
							{
								runLoop = false;
								startEncoderVal = AlexEncoder::counter - 4;//Point to same menu item
								continue;
							}

							ESP_LOGI("","Saving record...");
							saveRes = saveMesurementResult(measurementRes, false, false, selectedMesurementRecordNumber);
							break;
						}
						case MeasurementSaveMenuItems::BACK_TO_MEASUREMENT_RESULT:
						{
							return MeasurementSaveScreenResult::CANCEL;
							break;
						}
						default:
							halt("Halted. Unknown measurement save option");
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
								ESP_LOGI("","Saving record...");
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
							halt("Halted. Unknown measurement save option");
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
	}
}

void renderMeasuredResult(MeasuredResult &res)
{
	int16_t startEncoderVal = AlexEncoder::counter;
	int8_t prevPageIndex = -1;
	display->fillScreen(BLACK);

	while (true)
	{
		if (isApiRequestReceived)
			return;

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

		// displayManager.drawMeasuredScreen(resultPageIndex, res);
		#pragma region // display interaction
		// int8_t resultPageIndex = getInputParamsArrayInt(1);
		bool isReadingsValid = res.sensor0Time != MEASUREMENTS_IS_INVALID_VAL;

		if (!isReadingsValid)
		{
			int16_t yMargin = 25;
			int16_t step = 12;
			drawStringHCentered("Measurement result", yMargin);
			yMargin += step;
			drawStringHCentered("is invalid", yMargin);
			yMargin += step + 8;

			drawStringHCentered("Please check if shutter", yMargin);
			yMargin += step;
			drawStringHCentered("movement settings was", yMargin);
			yMargin += step;
			drawStringHCentered("set correctly", yMargin);
		}
		else
		{
			// result by sensor, shutter speed
			if (resultPageIndex >= 0 && resultPageIndex <= 1 && prevPageIndex != resultPageIndex)
			{
				display->fillScreen(BLACK);

				// double sensorTime = getInputParamsArrayFloat(2);
				double sensorTime = resultPageIndex == 0 ? res.sensor0Time : res.sensor1Time;

				drawStringHCentered("Sensor " + String(resultPageIndex + 1) + " summary", 15);

				if (sensorTime > 0) // if any data taken
				{
					display->setCursor(5, 35);

					if (sensorTime < 1.0) // less than a millisecond
					{
						display->printf("Time: %.3f ms", sensorTime);
					}
					else if (sensorTime > 1000.0) // More than a second
					{
						display->printf("Time: %.2f s", sensorTime / 1000.0);
					}
					else // between 1ms and 1s
					{
						display->printf("Time: %.2f ms", sensorTime);
					}

					if (sensorTime <= 1000.0) // show speed and graph for sensor time only under 1 second
					{
						display->setCursor(5, 50);
						double shutterSpeed = 1000.0 / sensorTime;
						String speedLabel = "Speed: ";
						String speedLabelValue = "1/" + String(shutterSpeed, 2) + " s";
						display->print(speedLabel + speedLabelValue);

						uint16_t speedA = -1;
						uint16_t speedB = -1;

						for (size_t i = 0; i < SHUTTR_SPEEDS_COUNT; i++)
						{
							if (shutterSpeeds[i] <= shutterSpeed)
							{
								speedA = shutterSpeeds[i - 1];
								speedB = shutterSpeeds[i];
								break;
							}
						}

						display->drawXBitmap(SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX, 80, SensorResultGraphImagBits, SENSOR_RESULT_GRAPH_IMAGE_WIDTH, SENSOR_RESULT_GRAPH_IMAGE_HEIGHT, WHITE);
						display->setCursor(5, 105);
						display->printf("1/%d", speedA);
						String speedBstr = speedB == 1 ? String(speedB) : "1/" + String(speedB);
						Rect speedBRect = getStringRect(speedBstr.c_str(), 0, 0);
						display->setCursor(160 - speedBRect.width - 5, 105);
						display->printf(speedBstr.c_str());

						double range = speedA - speedB;
						double sensorSpeedOnRange = shutterSpeed - speedB;

						double resultPercents = 1.0 - sensorSpeedOnRange / range;
						double speedPointLeftMargin = SPEEDS_GRAPH_BAR_WIDTH_PX * resultPercents;

						int16_t circleX = SPEEDS_GRAPH_BAR_LEFT_MARGIN_PX + 2 + (int16_t)speedPointLeftMargin;
						int16_t circleY = 83;
						display->fillCircle(circleX, circleY, 3, GREEN);

						Rect speedLabelRect = getStringRect(speedLabelValue.c_str(), 0, 0);
						display->drawLine(circleX, circleY, 5 + speedLabelRect.width + 40, 52, GREEN);
						display->drawLine(5 + speedLabelRect.width + 40, 52, 45, 52, GREEN);
					}
				}
				else
				{
					if (sensorTime == SENSOR_LIGHT_IS_TOO_DIM)
					{
						drawStringHCentered("Light is too dim", 40);
					}

					if (sensorTime == SENSOR_LIGHT_IS_TOO_BRIGHT)
					{
						drawStringHCentered("Light is too bright", 40);
					}

					if (sensorTime == SENSOR_TIME_IS_TOO_SHORT)
					{
						drawStringHCentered("Sensor time is too short", 40);
					}
				}

				prevPageIndex = resultPageIndex;
				drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
			}

			// 1-st and 2-nd curtain speeds and timings
			if (resultPageIndex == 2 && prevPageIndex != resultPageIndex)
			{
				display->fillScreen(BLACK);
				uint8_t curPos = 22;

				if (res.curtain1spanAtime == BOTH_SENSORS_MEASUREMENTS_REQUIRED) // no curtain data
				{
					drawStringHCentered("No curtain data", 30);
					drawStringHCentered("Measurements from both", 55);
					drawStringHCentered("sensors are required", 70);
				}
				else if (res.curtain1spanAtime == NOT_AVAILABLE_FOR_LEAF_SHUTERS) // no curtain data
				{
					drawStringHCentered("Curtain data is not", 40);
					drawStringHCentered("available for", 55);
					drawStringHCentered("leaf shutters", 70);
				}
				else
				{
					drawStringHCentered("1st curtain", 15);
					const uint8_t lineSpacing = 11;

					display->setFont();

					display->setCursor(0, curPos);
					display->printf(" Speed b/w sen.: %1.2f m/s", res.curtain1spanAspeed);
					curPos += lineSpacing;

					display->setCursor(0, curPos);
					display->printf("  Time b/w sen.: %1.2f ms", res.curtain1spanAtime);
					curPos += lineSpacing;

					display->setCursor(0, curPos);
					display->printf(" Est. tot. time: %1.2f ms", res.curtain1TotalTime);
					curPos += lineSpacing + 15;

					display->setCursor(0, curPos);
					display->setFont(u8g2_font_6x13_tf);

					drawStringHCentered("2nd curtain", curPos);

					curPos += 5;

					display->setFont();

					display->setCursor(0, curPos);
					display->printf(" Speed b/w sen.: %1.2f m/s", res.curtain2spanAspeed);
					curPos += lineSpacing;

					display->setCursor(0, curPos);
					display->printf("  Time b/w sen.: %1.2f ms", res.curtain2spanAtime);
					curPos += lineSpacing;

					display->setCursor(0, curPos);
					display->printf(" Est. tot. time: %1.2f ms", res.curtain2TotalTime);
					curPos += lineSpacing + 5;

					display->setFont(u8g2_font_6x13_tf);
				}

				prevPageIndex = resultPageIndex;
				drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
			}

			if (resultPageIndex == 3 && prevPageIndex != resultPageIndex)
			{
				display->fillScreen(BLACK);

				mString<30> title;
				title += "Estimated slit width";

				drawStringHCentered(title, 15);
				const uint8_t lineSpacing = 11;
				uint8_t curPos = 22;

				display->setFont();
				display->setCursor(0, curPos);

				if (res.slitWidthSensor0 == BOTH_SENSORS_MEASUREMENTS_REQUIRED)
				{
					drawStringHCentered("Measurements from both", 27);
					drawStringHCentered("sensors are required", 37);
					curPos += lineSpacing * 2;
				}
				else if (res.slitWidthSensor0 == NOT_AVAILABLE_FOR_LEAF_SHUTERS)
				{
					drawStringHCentered("Not available", 27);
					drawStringHCentered("for leaf shutters", 37);
					curPos += lineSpacing * 2;
				}
				else
				{
					display->printf(" By sensor 1: %1.2f mm", res.slitWidthSensor0);
					curPos += lineSpacing;
					display->setCursor(0, curPos);
					display->printf(" By sensor 2: %1.2f mm", res.slitWidthSensor1);
					curPos += lineSpacing;
					display->setCursor(0, curPos);
					display->printf(" On average: %1.2f mm", res.slitWidthAverage);
				}

				curPos += lineSpacing + 10;

				title = "Other data";
				display->setFont(u8g2_font_6x13_tf);
				drawStringHCentered(title, curPos + 5);
				display->setFont();

				curPos += lineSpacing + 3;
				display->setCursor(0, curPos);
				display->printf(" Used sensor.: %s", SensorTypeStr[(uint8_t)res.usedSensorType]);

				curPos += lineSpacing;
				display->setCursor(0, curPos);
				display->printf(" Sel.curt.mov.: %s", CurtainMovementItemsStr[(int8_t)res.selectedCurtainMovement]);

				display->setFont(u8g2_font_6x13_tf);

				prevPageIndex = resultPageIndex;
				drawNavBar(resultPageIndex, RESULT_PAGES_COUNT);
			}
		}
		#pragma endregion // end of display interaction

		if (button.isClicked())
		{
			return;
		}
	}
}

void drawMeasuredScreen(uint32_t rawSensor0TimeTakenUs, uint32_t rawSensor1TimeTakenUs, uint32_t curtain1TimeUs, uint32_t curtain2TimeUs)
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

	// sensor0Max = 163;
	// sensor1Max = 20;
	// curtainMovement = CurtainMovement::HORISONTAL;

	bool sensor0DataOk = false;
	bool sensor1DataOk = false;
	// double rawSensor0TimeTakenUs = -1;
	// double rawSensor1TimeTakenUs = -1;
	double correctedSensor0TimeTakenUs = -1;
	double correctedSensor1TimeTakenUs = -1;

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
		correctedSensor0TimeTakenUs = getCorrectedSensorValue(rawSensor0TimeTakenUs, sensor0Max); // in microseconds
		res.sensor0Time = correctedSensor0TimeTakenUs / US_IN_MILLISECOND;												// in milliseconds

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
		correctedSensor1TimeTakenUs = getCorrectedSensorValue(rawSensor1TimeTakenUs, sensor1Max); // in microseconds
		res.sensor1Time = correctedSensor1TimeTakenUs / US_IN_MILLISECOND;												// in milliseconds

		if (res.sensor1Time < TOO_SHORT_SENSOR_TIME)
		{
			res.sensor1Time = SENSOR_TIME_IS_TOO_SHORT;
		}
		else
		{
			sensor1DataOk = true;
		}
	}

	ESP_LOGI("", "Sensor 0 max: %" PRIu16, sensor0Max);
	ESP_LOGI("", "Sensor 1 max: %" PRIu16, sensor1Max);
	ESP_LOGI("", "Sensor 0 raw time: %" PRIu32, rawSensor0TimeTakenUs);
	ESP_LOGI("", "Sensor 1 raw time: %" PRIu32, rawSensor1TimeTakenUs);
	ESP_LOGI("", "Sensor 0 corrected time: %" PRIu32, (uint32_t)correctedSensor0TimeTakenUs);
	ESP_LOGI("", "Sensor 1 corrected time: %" PRIu32, (uint32_t)correctedSensor1TimeTakenUs);
	ESP_LOGI("", "Curtain 1 time: %" PRIu32, curtain1TimeUs);
	ESP_LOGI("", "Curtain 2 time: %" PRIu32, curtain2TimeUs);

	CurtainTimings curtainTimings;
	double sensorDistance = 0;
	double frameSize = 0;

	if (curSensorIndex == -1)
	{
		halt("Sensor index is not set");
	}

	SensorUnitData curSensorData = sensorsData[curSensorIndex];
	res.usedSensorType = curSensorData.Type;

	ESP_LOGI("", "Sensor unit name: %s", SensorTypeStr[(uint8_t)curSensorData.Type]);

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
			curtainTimings.curtain1spanAtime = curtain1TimeUs;
			curtainTimings.curtain2spanAtime = curtain2TimeUs;

			if (curtainMovement == CurtainMovement::HORISONTAL)
			{
				frameSize = curSensorData.FrameWidth;										 // in millimiters
				sensorDistance = curSensorData.HorisontalSensorDistance; // in millimiters
			}
			else if (curtainMovement == CurtainMovement::VERTICAL)
			{
				frameSize = curSensorData.FrameHeight;								 // in millimiters
				sensorDistance = curSensorData.VerticalSensorDistance; // in millimiters
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

	// === ГЕНЕРАЦИЯ JSON ДЛЯ API ===
	cJSON *json = cJSON_CreateObject();

	if (json != NULL)
	{
		cJSON_AddStringToObject(json, "type", "measurement_result");

		// Проверяем, успешны ли измерения (если нет, отправляем флаг ошибки)
		bool isDataValid = (res.sensor0Time != MEASUREMENTS_IS_INVALID_VAL);
		cJSON_AddBoolToObject(json, "is_valid", isDataValid);

		if (isDataValid)
		{
			// Время прохождения шторок над сенсорами (мс)
			cJSON_AddNumberToObject(json, "sensor1_time_ms", res.sensor0Time);
			cJSON_AddNumberToObject(json, "sensor2_time_ms", res.sensor1Time);

			// Данные первой шторки
			cJSON_AddNumberToObject(json, "curtain1_speed_ms", res.curtain1spanAspeed);
			cJSON_AddNumberToObject(json, "curtain1_time_ms", res.curtain1spanAtime);
			cJSON_AddNumberToObject(json, "curtain1_total_time_ms", res.curtain1TotalTime);

			// Данные второй шторки
			cJSON_AddNumberToObject(json, "curtain2_speed_ms", res.curtain2spanAspeed);
			cJSON_AddNumberToObject(json, "curtain2_time_ms", res.curtain2spanAtime);
			cJSON_AddNumberToObject(json, "curtain2_total_time_ms", res.curtain2TotalTime);

			// Ширина щели
			cJSON_AddNumberToObject(json, "slit_width_s1_mm", res.slitWidthSensor0);
			cJSON_AddNumberToObject(json, "slit_width_s2_mm", res.slitWidthSensor1);
			cJSON_AddNumberToObject(json, "slit_width_avg_mm", res.slitWidthAverage);
		}
		else
		{
			// Можно добавить причину ошибки, если нужно
			cJSON_AddStringToObject(json, "error", "Invalid measurement data");
		}

		// Вывод в Serial
		char *json_str = cJSON_PrintUnformatted(json);

		if (json_str != NULL)
		{
			printf("%s\n", json_str);
			free(json_str); // Не забываем освобождать память!
		}

		cJSON_Delete(json);
	}
	// ==============================

	MeasurementSaveScreenResult saveScreenRes = MeasurementSaveScreenResult::OK;

	do
	{
		renderMeasuredResult(res);
		if (isApiRequestReceived) return; // ВЫХОД ДЛЯ API
		
		if (sensor0DataOk || sensor1DataOk)
		{
			saveScreenRes = drawMeasurementSaveScreen(res);
		} 

	} while (saveScreenRes == MeasurementSaveScreenResult::CANCEL && !isApiRequestReceived);
}

void drawMeasuringScreen()
{
		sensor0Max = 0;
		sensor1Max = 0;

		uint32_t sensor1ADCSamplesCounter = 0;
		bool isSensor1Opened = false;
		bool isSensor1Closed = false;
		uint32_t sensor2ADCSamplesCounter = 0;
		bool isSensor2Opened = false;
		bool isSensor2Closed = false;
		uint32_t time = 0;

		esp_err_t adcReadRes;
		uint32_t retNum = 0;
		uint8_t result[ADC_READ_LEN] = {0};
		memset(result, 0xcc, ADC_READ_LEN);

		const uint16_t resArrLength = UINT8_MAX + 1;
		uint16_t sensor1ResArr[resArrLength] = {};
		uint16_t sensor2ResArr[resArrLength] = {};
		uint8_t sensor1ResArrInd = 0;
		uint8_t sensor2ResArrInd = 0;
		uint32_t curtain1ADCSamplesCounter = 0;
		uint32_t curtain2ADCSamplesCounter = 0;
		int8_t curtain1firstOpenedSensor = -1;
		int8_t curtain2firstClosedSensor = -1;

		display->fillScreen(BLACK);
		display->setTextSize(2);
		drawStringHCentered("MEASURING", 40);
		display->setTextSize(1);
		drawStringHCentered("Release camera shutter", 60);

		ESP_ERROR_CHECK(adc_continuous_start(handle));

		// s_task_handle = xTaskGetCurrentTaskHandle();

		// while (1)
		// {

		/**
		 * This is to show you the way to use the ADC continuous mode driver event callback.
		 * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
		 * However in this example, the data processing (print) is slow, so you barely block here.
		 *
		 * Without using this event callback (to notify this task), you can still just call
		 * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
		 */
		// ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		// char unit[] = ADC_UNIT_STR(ADC_UNIT);

		while (true)
		{
			if (isApiRequestReceived) // ВЫХОД ДЛЯ API с остановкой АЦП
			{
				ESP_ERROR_CHECK(adc_continuous_stop(handle));
				return;
			}

			adcReadRes = adc_continuous_read(handle, result, ADC_READ_LEN, &retNum, UINT32_MAX);

			if (adcReadRes == ESP_OK)
			{
				// ESP_LOGI("TASK", "ret is %x, ret_num is %" PRIu32 " bytes", ret, ret_num);
				for (int i = 0; i < retNum; i += SOC_ADC_DIGI_RESULT_BYTES)
				{
					adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
					uint32_t chan_num = ADC_GET_CHANNEL(p);
					uint16_t adcVal = ADC_GET_DATA(p);

					if (chan_num == ADC_CHANNEL_1) // senosor 1
					{
						if (adcVal > sensor0Max)
						{
							sensor0Max = adcVal;
						}

						if (adcVal > SHUTTER_OPEN_LEVEL && !isSensor1Opened)
						{
							isSensor1Opened = true;
							// ESP_LOGI("", "On");
						}

						if (adcVal < sensor0Max - SHUTTER_OPEN_LEVEL && !isSensor1Closed)
						{
							isSensor1Closed = true;
							// ESP_LOGI("", "Off. ADC %" PRIu16 ", max: %" PRIi16, adcVal, sensor0Max);
						}

						if (isSensor1Opened && !isSensor1Closed)
						{
							sensor1ADCSamplesCounter++;
						}

						if (!isSensor1Closed || (isSensor1Closed && adcVal > 50))
						{
							sensor1ResArr[sensor1ResArrInd] = adcVal;
							sensor1ResArrInd++;
						}
					}
					else // sensor 2
					{
						if (adcVal > sensor1Max)
						{
							sensor1Max = adcVal;
						}

						if (adcVal > SHUTTER_OPEN_LEVEL && !isSensor2Opened)
						{
							isSensor2Opened = true;
							// ESP_LOGI("", "On");
						}

						if (adcVal < sensor1Max - SHUTTER_OPEN_LEVEL && !isSensor2Closed)
						{
							isSensor2Closed = true;
							// ESP_LOGI("", "Off. ADC %" PRIu16 ", max: %" PRIi16, adcVal, sensor0Max);
						}

						if (isSensor2Opened && !isSensor2Closed)
						{
							sensor2ADCSamplesCounter++;
						}

						if (!isSensor2Closed || (isSensor2Closed && adcVal > 50))
						{
							sensor2ResArr[sensor2ResArrInd] = adcVal;
							sensor2ResArrInd++;
						}
					}

					if ((isSensor1Opened && !isSensor2Opened) ||
							(!isSensor1Opened && isSensor2Opened))
					{
						curtain1ADCSamplesCounter++;
					}

					if ((!isSensor1Closed && isSensor2Closed) ||
								(isSensor1Closed && !isSensor2Closed))
					{
						curtain2ADCSamplesCounter++;
					}

					// /* Check the channel number validation, the data is invalid if the channel num exceed the maximum channel */
					// if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT))
					// {
					// ESP_LOGI(TAG, "Unit: %s, Channel: %" PRIu32 ", Value: %" PRIu32, unit, chan_num, data);
					// }
					// else
					// {
					//   ESP_LOGW(TAG, "Invalid data [%s_%" PRIu32 "_%" PRIx32 "]", unit, chan_num, data);
					// }
				}

				/**
				 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
				 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
				 * usually you don't need this delay (as this task will block for a while).
				 */
				// vTaskDelay(1);
			}
			else if (adcReadRes == ESP_ERR_TIMEOUT)
			{
				// We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
				ESP_LOGW("", "ADC timed out");
				break;
			}

			if (button.isClicked())
			{
				ESP_ERROR_CHECK(adc_continuous_stop(handle));
				return;
			}
			else if (((isSensor1Opened && isSensor1Closed) ||
								(isSensor2Opened && isSensor2Closed))) // Check if at least one sensor has data
			{
				if (time == 0)
				{
					time = millis();
				}

				// wait for other sensors to get data
				if (millis() - time > 500)
				{
					ESP_ERROR_CHECK(adc_continuous_stop(handle));
					ESP_LOGI("", "ADC conversions taken for sensor 1: %" PRIu32, sensor1ADCSamplesCounter);
					ESP_LOGI("", "ADC conversions taken for sensor 2: %" PRIu32, sensor2ADCSamplesCounter);

					drawMeasuredScreen(sensor1ADCSamplesCounter * ONE_ADC_CONVERSION_TIME_US,
														 sensor2ADCSamplesCounter * ONE_ADC_CONVERSION_TIME_US,
														 (curtain1ADCSamplesCounter / 2) * ONE_ADC_CONVERSION_TIME_US,
														 (curtain2ADCSamplesCounter / 2) * ONE_ADC_CONVERSION_TIME_US);

					/* for (uint8_t i = 0; i < resArrLength - 1; i++)
					{
						ESP_LOGI("", "$%" PRIu16 " %" PRIu16 ";", sensor1ResArr[i], sensor2ResArr[i]);
					} */

					return;
				}
			}
		}
		// ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}

CurtainMovementSelectionScreenResult drawCurtainMovementSelectionScreen()
{
  display->fillScreen(BLACK);
  drawStringHCentered("Select curtain movement", 20);
  
  int16_t startEncoderVal = AlexEncoder::counter;
  uint8_t xMargin = 50;
  uint8_t yMargin = 55;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
	const uint8_t totalMenuItemsCount = CURTAN_MOVEMENT_SELECTION_SCREEN_OPTIONS_COUNT + 1;//+1 for 'Go back' item

	for (uint8_t i = 0; i < totalMenuItemsCount; i++)
	{
		display->setCursor(xMargin, yMargin + (ySpacing * i));

		if (i == totalMenuItemsCount - 1)
		{
			display->println("Go Back");
		}
		else
		{
			display->println(GetCurtainMovementItemTitle((CurtainMovement)i));
		}
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int8_t resultMenuItemIndex = 0;
  int8_t prevSelectedMenuItemIndex = 0;

  while (true)
  {
		if (isApiRequestReceived) return CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU; // ВЫХОД ДЛЯ API

    int16_t resultMenuItemIndex = AlexEncoder::counter - startEncoderVal;

		if (resultMenuItemIndex > totalMenuItemsCount - 1)
		{
			startEncoderVal = AlexEncoder::counter - (totalMenuItemsCount - 1);
			resultMenuItemIndex = totalMenuItemsCount - 1;
		}

    if (resultMenuItemIndex < 0)
    {
      resultMenuItemIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    // displayManager.drawCurtainMovementSelectionScreen(resultMenuItemIndex);

    if (prevSelectedMenuItemIndex != resultMenuItemIndex)
    {
      display->setTextColor(BLACK);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * prevSelectedMenuItemIndex));
      display->print("->");

      display->setTextColor(WHITE);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * resultMenuItemIndex));
      display->print("->");

      prevSelectedMenuItemIndex = resultMenuItemIndex;
    }

    if (button.isClicked())
    {
			if (resultMenuItemIndex == totalMenuItemsCount - 1)//'Go back' selected
			{
				return CurtainMovementSelectionScreenResult::GO_BACK;
			}
			
      curtainMovement = (CurtainMovement)resultMenuItemIndex;
      drawMeasuringScreen();
			return CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU;
		}
  }
}

void drawSensorSelectionScreen()
{
	const char *options[sensorsDataArraySize + 1] = {};

	for (size_t i = 0; i < sensorsDataArraySize; i++)
	{
		options[i] = SensorTypeStr[i];
	}

	options[sensorsDataArraySize] = "Go Back";

	while (true)
	{
		if (isApiRequestReceived)
			return; // ВЫХОД ДЛЯ API

		int16_t selectedOptionIndex = drawMessageScreen("Select sensor type", sensorsDataArraySize + 1, options);
		if (isApiRequestReceived)
			return; // ВЫХОД ДЛЯ API

		ESP_LOGI("", "Selected sensor index: %" PRIi16, selectedOptionIndex);
	
		if (selectedOptionIndex == sensorsDataArraySize) //'Go back' selected
		{
			return;
		}
	
		curSensorIndex = selectedOptionIndex;
		auto menuRes = drawCurtainMovementSelectionScreen();

		if (isApiRequestReceived)
			return; // ВЫХОД ДЛЯ API

		if (menuRes == CurtainMovementSelectionScreenResult::GO_BACK)
		{
			continue;
		}

		if (menuRes == CurtainMovementSelectionScreenResult::GO_TO_MAIN_MENU)
		{
			return;
		}
	}
}

void drawSensorSignalLevelBar(uint8_t &x, uint8_t &y, uint8_t sensorNumber, uint16_t curSensorValue)
{
	static int8_t prevSensor1SignalStatus = -1;
	static int8_t prevSensor2SignalStatus = -1;
	static int8_t prevSensor1ActualBarWidth = 0;
	static int8_t prevSensor2ActualBarWidth = 0;
	static const char *sensorStatuses[3] = {"Too dim","Too bright","OK"};
	const uint8_t barWidthPx = 80;
	const uint8_t barX = 10;
	double sensorValueRate = curSensorValue / (double)MAX_SIGNAL_LEVEL;
	sensorValueRate = sensorValueRate > 1.0 ? 1.0 : sensorValueRate;
	x = 15;
	y += 20;
	display->setCursor(x, y);
	display->printf("Sensor %i signal level", sensorNumber);
	y += 5;
	display->drawRect(barX, y, barWidthPx, 10, WHITE);
	int16_t newBarWidth = (barWidthPx - 2) * sensorValueRate;

	if (sensorNumber == 1 && newBarWidth != prevSensor1ActualBarWidth)
	{
		if (newBarWidth > prevSensor1ActualBarWidth) // add to what we have
		{
			display->fillRect(barX + 1 + prevSensor1ActualBarWidth, y + 1, newBarWidth - prevSensor1ActualBarWidth, 8, WHITE);
		}
		else // draw black upon what we have
		{
			display->fillRect(barX + 1 + prevSensor1ActualBarWidth - (prevSensor1ActualBarWidth - newBarWidth),
												y + 1, prevSensor1ActualBarWidth - newBarWidth, 8, BLACK);
		}

		prevSensor1ActualBarWidth = newBarWidth;
	}

	if (sensorNumber == 2 && newBarWidth != prevSensor2ActualBarWidth)
	{
		if (newBarWidth > prevSensor2ActualBarWidth) // add to what we have
		{
			display->fillRect(barX + 1 + prevSensor2ActualBarWidth, y + 1, newBarWidth - prevSensor2ActualBarWidth, 8, WHITE);
		}
		else // draw black upon what we have
		{
			display->fillRect(barX + 1 + prevSensor2ActualBarWidth - (prevSensor2ActualBarWidth - newBarWidth),
												y + 1, prevSensor2ActualBarWidth - newBarWidth, 8, BLACK);
		}

		prevSensor2ActualBarWidth = newBarWidth;
	}

	x += barWidthPx;
	y += 9;

	int8_t sensorStatus = 0;

	if (curSensorValue <= MIN_ALLOWED_SIGNAL_LEVEL)
	{
		sensorStatus = 0;
	}
	else if (curSensorValue > MAX_ALLOWED_SIGNAL_LEVEL)
	{
		sensorStatus = 1;
	}
	else
	{
		sensorStatus = 2;
	}

	if (sensorNumber == 1 && sensorStatus != prevSensor1SignalStatus)
	{
		display->setTextColor(BLACK); // erase old text
		display->setCursor(x, y);
		display->print(sensorStatuses[prevSensor1SignalStatus == -1 ? 0 : prevSensor1SignalStatus]);

		prevSensor1SignalStatus = sensorStatus;
	}

	if (sensorNumber == 2 && sensorStatus != prevSensor2SignalStatus)
	{
		display->setTextColor(BLACK); // erase old text
		display->setCursor(x, y);
		display->print(sensorStatuses[prevSensor2SignalStatus == -1 ? 0 : prevSensor2SignalStatus]);

		prevSensor2SignalStatus = sensorStatus;
	}

	display->setCursor(x, y);
	display->setTextColor(WHITE);
	display->print(sensorStatuses[sensorStatus]);
}

void drawLightCheckScreen()
{
	uint32_t sensor1TotalADCSamplesCounter = 0;
	uint32_t sensor1OpenADCSamplesCounter = 0;
	uint32_t sensor2TotalADCSamplesCounter = 0;
	uint32_t sensor2OpenADCSamplesCounter = 0;
	const uint16_t displayUpdateSamplesCount = 8000;//200ms interval

	esp_err_t adcReadRes;
	uint32_t retNum = 0;
	uint8_t result[ADC_READ_LEN] = {0};
	memset(result, 0xcc, ADC_READ_LEN);

	const char *lightQualityBrightnessLabel = "Light quality";
	char *lightQualityStatusesStr[3] = {"Unknown", "Ok", "Bad"};
	display->fillScreen(BLACK);
	
	uint8_t barX = 65;
	uint8_t barY = 52;
	drawSensorSignalLevelBar(barX, barY, 1, 0); //draw zero value to force ful bar visibility
	drawSensorSignalLevelBar(barX, barY, 2, 0); // draw zero value to force ful bar visibility

	while (true)
	{
		if (isApiRequestReceived) isApiRequestReceived = false; // Сбрасываем, если мы уже здесь

		sensor0Max = 0;
		sensor1Max = 0;
		sensor1TotalADCSamplesCounter = 0;
		sensor2TotalADCSamplesCounter = 0;
		sensor1OpenADCSamplesCounter = 0;
		sensor2OpenADCSamplesCounter = 0;
		ESP_ERROR_CHECK(adc_continuous_start(handle));

		while (sensor1TotalADCSamplesCounter < displayUpdateSamplesCount)
		{
			adcReadRes = adc_continuous_read(handle, result, ADC_READ_LEN, &retNum, UINT32_MAX);

			if (adcReadRes == ESP_OK)
			{
				for (int i = 0; i < retNum; i += SOC_ADC_DIGI_RESULT_BYTES)
				{
					adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result[i];
					uint32_t chan_num = ADC_GET_CHANNEL(p);
					uint16_t adcVal = ADC_GET_DATA(p);

					if (chan_num == ADC_CHANNEL_1) // senosor 1
					{
						if (adcVal > sensor0Max)
						{
							sensor0Max = adcVal;
						}

						if (adcVal > SHUTTER_OPEN_LEVEL)
						{
							sensor1OpenADCSamplesCounter++;
						}

						sensor1TotalADCSamplesCounter++;
					}
					else // sensor 2
					{
						if (adcVal > sensor1Max)
						{
							sensor1Max = adcVal;
						}

						if (adcVal > SHUTTER_OPEN_LEVEL)
						{
							sensor2OpenADCSamplesCounter++;
						}

						sensor2TotalADCSamplesCounter++;
					}
				}
			}
			else if (adcReadRes == ESP_ERR_TIMEOUT)
			{
				// We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
				ESP_LOGW("", "ADC timed out");
				break;
			}
		}

		ESP_ERROR_CHECK(adc_continuous_stop(handle));
		ESP_LOGI("", "Sen 1: %" PRIu32 ", sen 2: %" PRIu32 ", sen 1 max: %" PRIu16 ", sen 2 max: %" PRIu16,
						 sensor1OpenADCSamplesCounter, sensor2OpenADCSamplesCounter, sensor0Max, sensor1Max);


		#pragma region // display interaction

		static LightQualityStatus oldLightQualityStatus = LightQualityStatus::UNKNOWN;
		LightQualityStatus resLightQualityStatus = LightQualityStatus::UNKNOWN;
		LightQualityStatus sen1LightQualityStatus = LightQualityStatus::UNKNOWN;
		LightQualityStatus sen2LightQualityStatus = LightQualityStatus::UNKNOWN;

		if (sensor0Max > SHUTTER_OPEN_LEVEL && sensor0Max < MAX_ALLOWED_SIGNAL_LEVEL)
		{
			if (sensor1OpenADCSamplesCounter < displayUpdateSamplesCount - 100)
			{
				sen1LightQualityStatus = LightQualityStatus::BAD;
			}
			else
			{
				sen1LightQualityStatus = LightQualityStatus::OK;
			}
		}

		if (sensor1Max > SHUTTER_OPEN_LEVEL && sensor1Max < MAX_ALLOWED_SIGNAL_LEVEL)
		{
			if (sensor2OpenADCSamplesCounter < displayUpdateSamplesCount - 100)
			{
				sen2LightQualityStatus = LightQualityStatus::BAD;
			}
			else
			{
				sen2LightQualityStatus = LightQualityStatus::OK;
			}
		}

		if (sen1LightQualityStatus == LightQualityStatus::BAD ||
				sen2LightQualityStatus == LightQualityStatus::BAD)
		{
			resLightQualityStatus = LightQualityStatus::BAD;
		}
		else if (sen1LightQualityStatus == LightQualityStatus::OK &&
							sen2LightQualityStatus == LightQualityStatus::OK)
		{
			resLightQualityStatus = LightQualityStatus::OK;
		}
		else if (sen1LightQualityStatus == LightQualityStatus::OK &&
							 sen2LightQualityStatus == LightQualityStatus::UNKNOWN)
		{
			resLightQualityStatus = LightQualityStatus::OK;
		}
		else if (sen2LightQualityStatus == LightQualityStatus::OK &&
							 sen1LightQualityStatus == LightQualityStatus::UNKNOWN)
		{
			resLightQualityStatus = LightQualityStatus::OK;
		}
		else
		{
			resLightQualityStatus = LightQualityStatus::UNKNOWN;
		}

		uint8_t x = 16;
		uint8_t y = 20;

		drawStringHCentered("Light quality", y);
		y += 30;

		display->setTextSize(2);

		if (oldLightQualityStatus != resLightQualityStatus)
		{
			display->setTextColor(BLACK);
			drawStringHCentered(lightQualityStatusesStr[(int8_t)oldLightQualityStatus], y);
			oldLightQualityStatus = resLightQualityStatus;
		}

		uint16_t lightQualityStatusTextColors[3] = {LIGHTGREY, GREEN, RED};

		display->setCursor(x, y);
		display->setTextColor(lightQualityStatusTextColors[(int8_t)resLightQualityStatus]);
		drawStringHCentered(lightQualityStatusesStr[(int8_t)resLightQualityStatus], y);

		barX = 65;
		barY = 52;
		
		display->setTextColor(WHITE);
		display->setTextSize(1);
		drawSensorSignalLevelBar(barX, barY, 1, sensor0Max);
		drawSensorSignalLevelBar(barX, barY, 2, sensor1Max);
		
		#pragma endregion //display interaction

		// === ГЕНЕРАЦИЯ JSON ДЛЯ API ===
		cJSON *json = cJSON_CreateObject();

		if (json != NULL) 
		{
			cJSON_AddStringToObject(json, "type", "light_setup_data");
			cJSON_AddNumberToObject(json, "sensor1_level", sensor0Max / MAX_SIGNAL_LEVEL * 100); // in percents
			cJSON_AddNumberToObject(json, "sensor2_level", sensor1Max / MAX_SIGNAL_LEVEL * 100); // in percents

			const char* s1_status = (sensor0Max <= MIN_ALLOWED_SIGNAL_LEVEL) ? 
				SerialAPILightStatus::LIGHT_STATUS_TOO_DIMM : ((sensor0Max > MAX_ALLOWED_SIGNAL_LEVEL) ? 
					SerialAPILightStatus::LIGHT_STATUS_TOO_BRIGHT : SerialAPILightStatus::LIGHT_STATUS_OK);

			const char* s2_status = (sensor1Max <= MIN_ALLOWED_SIGNAL_LEVEL) ? 
				SerialAPILightStatus::LIGHT_STATUS_TOO_DIMM : ((sensor1Max > MAX_ALLOWED_SIGNAL_LEVEL) ? 
					SerialAPILightStatus::LIGHT_STATUS_TOO_BRIGHT : SerialAPILightStatus::LIGHT_STATUS_OK);

			cJSON_AddStringToObject(json, "sensor1_status", s1_status);
			cJSON_AddStringToObject(json, "sensor2_status", s2_status);
			cJSON_AddStringToObject(json, "light_quality", serialApiLightQualityStatusesStr[(int8_t)resLightQualityStatus]);

			char *json_str = cJSON_PrintUnformatted(json);
			if (json_str != NULL) {
				printf("%s\n", json_str);
				free(json_str);
			}
			cJSON_Delete(json);
		}
		// ==============================

		if (button.isClicked())
    {
      return;
    }
	}
}

void drawAboutScreen()
{
  display->fillScreen(BLACK);

  while (true)
  {
		if (isApiRequestReceived) return; // ВЫХОД ДЛЯ API

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

void drawViewRecordsScreen()
{
	while (true)
	{
		if (isApiRequestReceived)
		{
			return; // ВЫХОД ДЛЯ API
		}

		int32_t selectedRecordNumber = drawMeasurementResultRecordSelectionScreen();

		if (isApiRequestReceived)
		{
			return;											 // ВЫХОД ДЛЯ API
		}

    if (selectedRecordNumber == 0) //"Back" option selected
    {
      return;
    }

    StoredMeasuredResult selectedResult = getMeasurementStoredResultByNumber(selectedRecordNumber);

    if (selectedResult.recordNumber == -1)
    {
      halt("Halted. selectedRecordNumber is not found");
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

		renderMeasuredResult(resultToDisplay);

		if (isApiRequestReceived)
			return; // ВЫХОД ДЛЯ API

		const char *options[] = {"Go back to list", "Go to main menu", "Delete record"};
		int16_t selectedOptionIndex = drawMessageScreen("What's next?", 3, options);

		if (isApiRequestReceived)
			return; // ВЫХОД ДЛЯ API

		if (selectedOptionIndex == 0) // Go back to list
		{
			continue;
		}

		if (selectedOptionIndex == 1) // Go to main menu
		{
			return;
		}

		if (selectedOptionIndex == 2) // Delete record
		{
			const char *options[] = {"Yes", "No"};
			int16_t deleteOptionIndex = drawMessageScreen("Really delete?", 2, options);

			if (isApiRequestReceived)
				return; // ВЫХОД ДЛЯ API

			if (deleteOptionIndex == 0) // Delete record
			{
				bool deleteRes = deleteMeasurementStoredResultByNumber(selectedResult.recordNumber);
				char *msg;

				if (!deleteRes)
				{
					msg = "Failed to delete record";
				}
				else
				{
					msg = "Record deleted";
				}

				const char *deleteResOptions[] = {"OK"};
				drawMessageScreen(msg, 1, deleteResOptions);
			}
			continue; // go to records list selection
		}

		return;
  }
}

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

			// === ПЕРЕХВАТЧИК API ===
			if (isApiRequestReceived)
			{
				isApiRequestReceived = false;

				switch (apiRequestAction)
				{
					case ApiRequstAction::GO_TO_LIGHT_SETUP:
						drawLightCheckScreen();
						startEncoderVal = AlexEncoder::counter;
						break;

					case ApiRequstAction::GO_TO_MEASURE: 
						drawMeasuringScreen();
						startEncoderVal = AlexEncoder::counter;
						break;

					default:
						break;
				}
			}
			// =======================

			if (button.isClicked())
      {
        switch ((MainMenuItems)resultMenuItemIndex)
        {
        case MainMenuItems::MEASURE:
        {
          // drawCurtainMovementSelectionScreen();
					drawSensorSelectionScreen();
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
          halt("Halted. Unknown menu item");
          break;
        }

        startEncoderVal = AlexEncoder::counter;
        break;//break from while loop
      }
    }
  }
}

void initADC()
{
  continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

  adc_continuous_evt_cbs_t cbs = {
      .on_conv_done = s_conv_done_cb,
  };
  ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
}

void initStorage()
{
	ESP_LOGI("", "Initializing LittleFS");

	esp_vfs_littlefs_conf_t conf = {
			.base_path = LITTLEFS_BASE_PATH,
			.partition_label = LITTLEFS_PARTITION_LABEL,
			.format_if_mount_failed = true,
			.dont_mount = false,
	};

	esp_err_t ret = esp_vfs_littlefs_register(&conf);

	if (ret != ESP_OK)
	{
		if (ret == ESP_FAIL)
		{
			halt("Failed to mount or format filesystem");
		}
		else if (ret == ESP_ERR_NOT_FOUND)
		{
			halt("Failed to find LittleFS partition");
		}
		else
		{
			ESP_LOGE("", "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
			halt();
		}
		return;
	}

	size_t total = 0, used = 0;
	ret = esp_littlefs_info(conf.partition_label, &total, &used);

	if (ret != ESP_OK)
	{
		ESP_LOGE("", "Failed to get LittleFS partition information: (%s). Formatting...", esp_err_to_name(ret));
		esp_littlefs_format(conf.partition_label);
	}
	else
	{
		ESP_LOGI("", "Storage partition size: total: %d, used: %d", total, used);
	}

	// esp_vfs_littlefs_unregister(conf.partition_label);
	// ESP_LOGI(TAG, "LittleFS unmounted");

	struct stat st;
	int16_t blockSize = getMeasurementSaveRecordSize();
	uint8_t data[blockSize] = {};

	if (stat(RECORDS_FILE_PATH, &st) != 0) // if no records.bin file found
	{
		ESP_LOGI("", "No '" RECORDS_FILE_NAME "' file found. Initializing...");

		FILE *recordsFile = fopen(RECORDS_FILE_PATH, "w");

		for (int32_t i = 0; i < TOTAL_RECORDS_COUNT; i++)
		{
			size_t res = fwrite(data, blockSize, 1, recordsFile);

			if (res == 0)
			{
				halt("Halted. Failed to write inital data to '" RECORDS_FILE_NAME "'");
			}
		}

		ESP_LOGI("", "'" RECORDS_FILE_NAME "' initialized");
		fclose(recordsFile);

		ESP_LOGI("", "Verifing writen data...");

		recordsFile = fopen(RECORDS_FILE_PATH, "r");

		uint8_t buf[1] = {};

		for (int32_t i = 0; i < TOTAL_RECORDS_COUNT * blockSize; i++)
		{
			fread(buf, 1, 1, recordsFile);

			if (buf[0] != 0)
			{
				halt("Invalid data");
			}
		}

		fclose(recordsFile);
		ESP_LOGI("", "Data OK");
	}
}



void setup() 
{
	// pinMode(15, OUTPUT);
	// digitalWrite(15, HIGH);
	// delay(2000);
	// halt();

	display->begin();
  display->fillScreen(BLACK);
  display->setTextColor(WHITE);
  display->setFont(u8g2_font_6x13_tf);

  AlexEncoder::init(ENCODER_A_PIN, ENCODER_B_PIN);

  auto timer = timerBegin(1000000);
  timerAttachInterrupt(timer, &onTimer);
  timerAlarm(timer, US_IN_SECOND / USER_INPUT_POLLING_FREQ_HZ, true, 0);

  initADC();
	initStorage();

	storage.begin();
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

	drawMainMenu();
}