#pragma once

#include <Arduino.h>
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "SerialAPICommon.h"

// 32 байта для AES-256 (Симметричный ключ) - поменяйте на свои случайные числа!
const unsigned char AesKey[32] = {
    0x4f, 0x8a, 0x1c, 0xe3, 0x7b, 0x9d, 0x24, 0x55,
    0xa6, 0xf0, 0x38, 0xcc, 0x19, 0xd2, 0x4e, 0x8f,
    0xbb, 0x71, 0x05, 0x6a, 0x3d, 0x9c, 0x82, 0xe4,
    0xf5, 0x09, 0x27, 0x1b, 0xd6, 0xaf, 0x3e, 0x90};

// 16 байт инициализационный вектор (IV) - поменяйте на свои!
const unsigned char AesIvInit[16] = {
    0x2c, 0x9f, 0x5b, 0x11, 0xd8, 0x4a, 0x73, 0xe6,
    0x04, 0x8c, 0x39, 0xf2, 0x1d, 0x6e, 0xa5, 0x77};

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
    printf("\n");
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
  // На ESP32 с Flash Encryption (XTS-AES) буферы должны быть выровнены по 32 байта
  const size_t RX_BUF_SIZE = 2048;
  const size_t B64_BUF_SIZE = 1536; // С запасом под Base64 строку (512 байт = ~684 символа + \n)

  unsigned char *rxBuf = (unsigned char *)heap_caps_aligned_alloc(32, RX_BUF_SIZE, MALLOC_CAP_DMA);
  unsigned char *decBuf = (unsigned char *)heap_caps_aligned_alloc(32, RX_BUF_SIZE, MALLOC_CAP_DMA);
  char *b64Buf = (char *)malloc(B64_BUF_SIZE);

  if (!rxBuf || !decBuf || !b64Buf)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_STATUS_ERROR, "Memory allocation failed", -1);
    
    if (rxBuf)
      heap_caps_free(rxBuf);
    if (decBuf)
      heap_caps_free(decBuf);
    if (b64Buf)
      free(b64Buf);
    mbedtls_aes_free(&aesCtx);
    return;
  }

  
  // Отправка статуса готовности
  sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA, NULL, -1);
  
  uint32_t lastDataTime = millis();
  size_t totalReceived = 0;
  size_t bufferPos = 0;
  bool transferFinished = false;

  // 5. Основной цикл приема данных
  while (!transferFinished)
  {
    // Читаем строку посимвольно до '\n' (fgets на UART возвращает частичные строки)
    size_t b64Len = 0;
    memset(b64Buf, 0, B64_BUF_SIZE);
    
    while (b64Len < B64_BUF_SIZE - 1)
    {
      int c = getchar();
      if (c == EOF)
      {
        // Данных нет: проверяем таймаут окончания передачи здесь,
        // иначе при молчании клиента этот цикл никогда не завершится
        // и устройство "зависнет" в ожидании следующей строки
        if (totalReceived > 0 && (millis() - lastDataTime > 5000))
        {
          transferFinished = true;
          break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        continue;
      }
      
      lastDataTime = millis();
      
      if (c == '\n')
      {
        break; // Конец строки
      }
      
      if (c != '\r') // Игнорируем \r
      {
        b64Buf[b64Len++] = (char)c;
      }
    }
    
    if (transferFinished)
    {
      break;
    }
    
    b64Buf[b64Len] = '\0';
    size_t len = b64Len;

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
          int aesRet = mbedtls_aes_crypt_cbc(&aesCtx, MBEDTLS_AES_DECRYPT, processLen, iv, rxBuf, decBuf);
          if (aesRet != 0)
          {
            char errMsg[64];
            snprintf(errMsg, sizeof(errMsg), "AES decrypt err: %d", aesRet);
            sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
            free(b64Buf);
            heap_caps_free(rxBuf);
            heap_caps_free(decBuf);
            mbedtls_aes_free(&aesCtx);
            return;
          }

          // Записываем во флеш
          esp_err_t writeErr = esp_ota_write(updateHandle, decBuf, processLen);
          if (writeErr != ESP_OK)
          {
            char errMsg[64];
            snprintf(errMsg, sizeof(errMsg), "esp_ota_write failed: %s", esp_err_to_name(writeErr));
            sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
            free(b64Buf);
            heap_caps_free(rxBuf);
            heap_caps_free(decBuf);
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

    // Завершаем процесс, если данные не приходили более 5 секунд
    if (totalReceived > 0 && (millis() - lastDataTime > 5000))
    {
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  // 5.1 Обработка оставшихся данных в буфере
  if (bufferPos > 0)
  {
    // AES-CBC требует длину, кратную 16 байтам: "хвост" другой длины
    // расшифровать невозможно - значит, файл был подготовлен некорректно
    if (bufferPos % 16 != 0)
    {
      char errMsg[80];
      snprintf(errMsg, sizeof(errMsg), "Invalid final block size: %u (not multiple of 16)", (unsigned)bufferPos);
      sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
      mbedtls_aes_free(&aesCtx);
      heap_caps_free(rxBuf);
      heap_caps_free(decBuf);
      free(b64Buf);
      return;
    }

    // Расшифровываем и записываем остаток
    int aesRet = mbedtls_aes_crypt_cbc(&aesCtx, MBEDTLS_AES_DECRYPT, bufferPos, iv, rxBuf, decBuf);
    if (aesRet != 0)
    {
      char errMsg[64];
      snprintf(errMsg, sizeof(errMsg), "AES decrypt final block err: %d", aesRet);
      sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
      mbedtls_aes_free(&aesCtx);
      heap_caps_free(rxBuf);
      heap_caps_free(decBuf);
      free(b64Buf);
      return;
    }

    esp_err_t writeErr = esp_ota_write(updateHandle, decBuf, bufferPos);
    if (writeErr != ESP_OK)
    {
      char errMsg[64];
      snprintf(errMsg, sizeof(errMsg), "esp_ota_write final block failed: %s", esp_err_to_name(writeErr));
      sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
      mbedtls_aes_free(&aesCtx);
      heap_caps_free(rxBuf);
      heap_caps_free(decBuf);
      free(b64Buf);
      return;
    }

    totalReceived += bufferPos;
    bufferPos = 0;
  }

  // 6. Освобождение памяти
  mbedtls_aes_free(&aesCtx);
  heap_caps_free(rxBuf);
  heap_caps_free(decBuf);
  free(b64Buf);

  // 7. Завершение OTA
  esp_err_t endErr = esp_ota_end(updateHandle);
  if (endErr != ESP_OK)
  {
    char errMsg[64];
    snprintf(errMsg, sizeof(errMsg), "esp_ota_end failed: %s", esp_err_to_name(endErr));
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
    return;
  }

  // 8. Переключение загрузочного раздела и перезагрузка
  esp_err_t bootErr = esp_ota_set_boot_partition(updatePartition);
  if (bootErr == ESP_OK)
  {
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_SUCCESS, 
      "Firmware update successful. Rebooting...", -1);
    // Даём время клиенту прочитать ответ перед перезагрузкой
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
  }
  else
  {
    char errMsg[64];
    snprintf(errMsg, sizeof(errMsg), "esp_ota_set_boot_partition failed: %s", esp_err_to_name(bootErr));
    sendOtaJsonResponse(SerialAPIResponse::API_RESPONSE_FIRMWARE_UPDATE_FAILED, errMsg, -1);
  }
}