#pragma once

#include <Arduino.h>
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "esp_ota_ops.h"
#include "cJSON.h"
#include "SerialAPICommon.h"

// 32 байта для AES-256 (Симметричный ключ) - поменяйте на свои случайные числа!
const unsigned char AesKey[32] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};

// 16 байт инициализационный вектор (IV) - поменяйте на свои!
const unsigned char AesIvInit[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// Вспомогательная функция для унифицированной отправки JSON
static inline void sendOtaJsonResponse(const char *status, const char *message, int bytesReceived)
{
  cJSON *json = cJSON_CreateObject();
  if (json == NULL)
    return; // Защита от нехватки памяти

  cJSON_AddStringToObject(json, "cmd", SerialAPIRequestAction::API_REQUEST_FIRMWARE_UPDATE);
  cJSON_AddStringToObject(json, "status", status);

  if (message != NULL)
  {
    cJSON_AddStringToObject(json, "message", message);
  }

  if (bytesReceived >= 0)
  {
    cJSON_AddNumberToObject(json, "bytesReceived", bytesReceived);
  }

  char *str = cJSON_PrintUnformatted(json);

  if (str != NULL)
  {
    printf("%s\n", str);
    fflush(stdout);
    free(str);
  }

  cJSON_Delete(json);
}

void startFirmwareUpdate()
{
  // 1. Поиск раздела для обновления
  const esp_partition_t *updatePartition = esp_ota_get_next_update_partition(NULL);

  if (updatePartition == NULL)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_STATUS_ERROR, "Failed to get update partition", -1);
    return;
  }

  // 2. Инициализация OTA
  esp_ota_handle_t updateHandle = 0;

  if (esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &updateHandle) != ESP_OK)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_STATUS_ERROR, "esp_ota_begin failed", -1);
    return;
  }

  // 3. Инициализация AES контекста
  mbedtls_aes_context aesCtx;
  mbedtls_aes_init(&aesCtx);
  // 256 бит (32 байта) — длина ключа
  mbedtls_aes_setkey_dec(&aesCtx, AesKey, 256);

  // Копируем IV в локальный массив, так как mbedtls_aes_crypt_cbc изменяет его в процессе
  unsigned char iv[16];
  memcpy(iv, AesIvInit, 16);

  // 4. Выделение памяти под буферы
  const size_t RX_BUF_SIZE = 1024;
  const size_t B64_BUF_SIZE = 1536; // С запасом под Base64 строку (512 байт = ~684 символа + \n)

  unsigned char *rxBuf = (unsigned char *)malloc(RX_BUF_SIZE);
  unsigned char *decBuf = (unsigned char *)malloc(RX_BUF_SIZE);
  char *b64Buf = (char *)malloc(B64_BUF_SIZE);

  if (!rxBuf || !decBuf || !b64Buf)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_STATUS_ERROR, "Memory allocation failed", -1);
    
    if (rxBuf)
      free(rxBuf);
    if (decBuf)
      free(decBuf);
    if (b64Buf)
      free(b64Buf);
    mbedtls_aes_free(&aesCtx);
    return;
  }

  
  // Отправка статуса готовности
  sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA, NULL, -1);
  
  // Отключаем буферизацию стандартного потока
  setvbuf(stdin, NULL, _IONBF, 0);
  
  uint32_t lastDataTime = millis();
  size_t totalReceived = 0;
  size_t bufferPos = 0;

  // 5. Основной цикл приема данных
  while (1)
  {
    // Очищаем буфер перед новым чтением
    memset(b64Buf, 0, B64_BUF_SIZE);

    // fgets ждет появления строки с '\n' в конце (или заполнения буфера)
    char *line = fgets(b64Buf, B64_BUF_SIZE, stdin);

    if (line != NULL)
    {
      lastDataTime = millis(); // Сброс таймера таймаута

      // Удаляем символы переноса строки (\n и \r) с конца прочитанной строки
      size_t len = strlen(b64Buf);
      while (len > 0 && (b64Buf[len - 1] == '\n' || b64Buf[len - 1] == '\r'))
      {
        b64Buf[len - 1] = '\0';
        len--;
      }

      if (len > 0)
      {
        size_t decodedLen = 0;
        unsigned char decodedTmp[1024];

        // Декодируем Base64 строку в бинарные данные
        int ret = mbedtls_base64_decode(decodedTmp, sizeof(decodedTmp), &decodedLen, (const unsigned char *)b64Buf, len);

        if (ret == 0 && decodedLen > 0)
        {
          // Переносим декодированные байты в конец буфера AES
          memcpy(rxBuf + bufferPos, decodedTmp, decodedLen);
          bufferPos += decodedLen;

          // AES CBC работает с блоками кратными 16 байт
          while (bufferPos >= 16)
          {
            size_t processLen = (bufferPos / 16) * 16;

            // Расшифровываем накопленный блок
            mbedtls_aes_crypt_cbc(&aesCtx, MBEDTLS_AES_DECRYPT, processLen, iv, rxBuf, decBuf);

            // Записываем во флеш
            if (esp_ota_write(updateHandle, decBuf, processLen) != ESP_OK)
            {
              sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, "esp_ota_write failed", -1);
              free(b64Buf);
              free(rxBuf);
              free(decBuf);
              mbedtls_aes_free(&aesCtx);
              return;
            }

            // Переносим бинарный "хвост" в начало
            size_t remainingBin = bufferPos - processLen;

            if (remainingBin > 0)
            {
              memmove(rxBuf, rxBuf + processLen, remainingBin);
            }
            
            bufferPos = remainingBin;

            totalReceived += processLen;

            // Отправка ACK
            sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK, NULL, totalReceived);
          }
        }
        else
        {
          char errMsg[64];
          snprintf(errMsg, sizeof(errMsg), "Base64 decode err: %d", ret);
          sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_STATUS_ERROR, errMsg, -1);
        }
      }
    }

    // Завершаем процесс, если данные не приходили более 2 секунд
    if (totalReceived > 0 && (millis() - lastDataTime > 2000))
    {
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  // 6. Освобождение памяти
  mbedtls_aes_free(&aesCtx);
  free(rxBuf);
  free(decBuf);
  free(b64Buf);

  // 7. Завершение OTA
  if (esp_ota_end(updateHandle) != ESP_OK)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, "Firmware update failed", -1);
    return;
  }

  // 8. Переключение загрузочного раздела и перезагрузка
  if (esp_ota_set_boot_partition(updatePartition) == ESP_OK)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_SUCCESS, 
      "Firmware update successful.", -1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    // esp_restart();
  }
  else
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, "Failed to set boot partition", -1);
  }
}