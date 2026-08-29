#pragma once
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_random.h"

class SerialNumber
{
private:
  static const char *const TAG;
  static const char *const NVS_NAMESPACE;
  static const char *const NVS_KEY;
  static const uint8_t SERIAL_NUMBER_LENGTH;

  static char serialNumber[11]; // 10 chars + null terminator

  static void generateSerialNumber(char *out, uint8_t length)
  {
    const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const uint8_t charsetSize = sizeof(charset) - 1; // exclude null terminator

    // Use hardware RNG for better randomness
    for (uint8_t i = 0; i < length; i++)
    {
      out[i] = charset[esp_random() % charsetSize];
    }
    out[length] = '\0';
  }

public:
  static void init()
  {
    // Initialize NVS flash
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
      // NVS partition was truncated and needs to be erased
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    nvs_handle_t handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
      return;
    }

    // Try to read existing serial number
    size_t length = SERIAL_NUMBER_LENGTH + 1;
    err = nvs_get_str(handle, NVS_KEY, serialNumber, &length);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
      // Serial number doesn't exist yet - generate and save it
      generateSerialNumber(serialNumber, SERIAL_NUMBER_LENGTH);
      err = nvs_set_str(handle, NVS_KEY, serialNumber);
      if (err != ESP_OK)
      {
        ESP_LOGE(TAG, "Failed to save serial number: %s", esp_err_to_name(err));
      }
      else
      {
        err = nvs_commit(handle);
        if (err != ESP_OK)
        {
          ESP_LOGE(TAG, "Failed to commit serial number: %s", esp_err_to_name(err));
        }
        else
        {
          ESP_LOGI(TAG, "New serial number generated: %s", serialNumber);
        }
      }
    }
    else if (err == ESP_OK)
    {
      ESP_LOGI(TAG, "Serial number loaded: %s", serialNumber);
    }
    else
    {
      ESP_LOGE(TAG, "Error reading serial number: %s", esp_err_to_name(err));
    }

    nvs_close(handle);
  }

  static const char *get()
  {
    return serialNumber;
  }
};

inline const char *const SerialNumber::TAG = "SerialNumber";
inline const char *const SerialNumber::NVS_NAMESPACE = "device";
inline const char *const SerialNumber::NVS_KEY = "serial";
inline const uint8_t SerialNumber::SERIAL_NUMBER_LENGTH = 10;
inline char SerialNumber::serialNumber[11] = {0};