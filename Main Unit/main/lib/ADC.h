#pragma once

#include <Arduino.h>
#include "esp_adc/adc_continuous.h"
#include "soc/soc_caps.h"
#include "esp_log.h"
#include <inttypes.h>
#include "freertos/FreeRTOS.h"

static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
adc_continuous_handle_t handle = NULL;
#define ADC_CONVERSION_FREQ_HZ 1000 * 1000
#define ADC_UNIT ADC_UNIT_1
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_BIT_WIDTH SOC_ADC_DIGI_MAX_BITWIDTH

#define READ_LEN 4096

volatile bool testFlag1 = false;
volatile bool testFlag2 = false;

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
	// digitalWrite(TEST_PIN_1, testFlag1);
	// testFlag1 = !testFlag1;

	BaseType_t mustYield = pdFALSE;
	// Notify that ADC continuous driver has done enough number of conversions
	// vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

	return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
	adc_continuous_handle_t handle = NULL;

	adc_continuous_handle_cfg_t adc_config = {
			.max_store_buf_size = 16384,
			.conv_frame_size = READ_LEN};
	ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

	adc_continuous_config_t dig_cfg = {
			.sample_freq_hz = ADC_CONVERSION_FREQ_HZ,
			.conv_mode = ADC_CONV_MODE,
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

void initADC()
{
	continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

	adc_continuous_evt_cbs_t cbs = {
			.on_conv_done = s_conv_done_cb,
	};
	ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
}
