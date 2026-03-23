#pragma once

#include <Arduino.h>
#include "mbedtls/aes.h"
#include "mbedtls/base64.h"
#include "esp_ota_ops.h"

// 32 байта для AES-256 (Симметричный ключ) - поменяйте на свои случайные числа!
const unsigned char aes_key[32] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};

// 16 байт инициализационный вектор (IV) - поменяйте на свои!
const unsigned char aes_iv_init[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

void startFirmwareUpdate()
{
  static const char *TAG = "SERIAL_OTA";

  ESP_LOGI(TAG, "Starting OTA. Preparing partition...");

  // 1. Поиск раздела для обновления
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition == NULL)
  {
    ESP_LOGE(TAG, "Failed to get update partition");
    return;
  }

  // 2. Инициализация OTA
  esp_ota_handle_t update_handle = 0;
  if (esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle) != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_begin failed");
    return;
  }

  // 3. Инициализация AES контекста
  mbedtls_aes_context aes_ctx;
  mbedtls_aes_init(&aes_ctx);
  // 256 бит (32 байта) — длина ключа
  mbedtls_aes_setkey_dec(&aes_ctx, aes_key, 256);

  // Копируем IV в локальный массив, так как mbedtls_aes_crypt_cbc изменяет его в процессе
  unsigned char iv[16];
  memcpy(iv, aes_iv_init, 16);

  // 4. Выделение памяти под буферы
  const size_t RX_BUF_SIZE = 1024;
  const size_t B64_BUF_SIZE = 1536; // С запасом под Base64 строку (512 байт = ~684 символа + \n)

  unsigned char *rx_buf = (unsigned char *)malloc(RX_BUF_SIZE);
  unsigned char *dec_buf = (unsigned char *)malloc(RX_BUF_SIZE);
  char *b64_buf = (char *)malloc(B64_BUF_SIZE);

  if (!rx_buf || !dec_buf || !b64_buf)
  {
    ESP_LOGE(TAG, "Memory allocation failed");
    if (rx_buf)
      free(rx_buf);
    if (dec_buf)
      free(dec_buf);
    if (b64_buf)
      free(b64_buf);
    mbedtls_aes_free(&aes_ctx);
    return;
  }

  // Отключаем буферизацию стандартного потока
  setvbuf(stdin, NULL, _IONBF, 0);

  // ВАЖНО: Мы убрали fcntl и O_NONBLOCK, так как fgets
  // сам корректно читает данные до появления символа '\n'

  printf("FIRMWARE_UPDATE_READY\n");
  fflush(stdout);

  uint32_t last_data_time = millis();
  size_t total_received = 0;
  size_t buffer_pos = 0;

  // 5. Основной цикл приема данных
  while (1)
  {
    // Очищаем буфер перед новым чтением
    memset(b64_buf, 0, B64_BUF_SIZE);

    // fgets ждет появления строки с '\n' в конце (или заполнения буфера)
    char *line = fgets(b64_buf, B64_BUF_SIZE, stdin);

    if (line != NULL)
    {
      last_data_time = millis(); // Сброс таймера таймаута

      // Удаляем символы переноса строки (\n и \r) с конца прочитанной строки
      size_t len = strlen(b64_buf);
      while (len > 0 && (b64_buf[len - 1] == '\n' || b64_buf[len - 1] == '\r'))
      {
        b64_buf[len - 1] = '\0';
        len--;
      }

      if (len > 0)
      {
        size_t decoded_len = 0;
        unsigned char decoded_tmp[1024];

        // Декодируем Base64 строку в бинарные данные
        int ret = mbedtls_base64_decode(decoded_tmp, sizeof(decoded_tmp), &decoded_len, (const unsigned char *)b64_buf, len);

        if (ret == 0 && decoded_len > 0)
        {
          // Переносим декодированные байты в конец буфера AES
          memcpy(rx_buf + buffer_pos, decoded_tmp, decoded_len);
          buffer_pos += decoded_len;

          // AES CBC работает с блоками кратными 16 байт
          while (buffer_pos >= 16)
          {
            size_t process_len = (buffer_pos / 16) * 16;

            // Расшифровываем накопленный блок
            mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, process_len, iv, rx_buf, dec_buf);

            // Записываем во флеш
            if (esp_ota_write(update_handle, dec_buf, process_len) != ESP_OK)
            {
              ESP_LOGE(TAG, "esp_ota_write failed");
              free(b64_buf);
              free(rx_buf);
              free(dec_buf);
              mbedtls_aes_free(&aes_ctx);
              return;
            }

            // Переносим бинарный "хвост" в начало
            size_t remaining_bin = buffer_pos - process_len;
            if (remaining_bin > 0)
            {
              memmove(rx_buf, rx_buf + process_len, remaining_bin);
            }
            buffer_pos = remaining_bin;

            total_received += process_len;

            // Отправляем ACK (подтверждаем запись)
            printf("ACK:%zu\n", total_received);
            fflush(stdout);
          }
        }
        else
        {
          ESP_LOGE(TAG, "Base64 decode err: %d", ret);
        }
      }
    }

    // Завершаем процесс, если данные не приходили более 3 секунд
    if (total_received > 0 && (millis() - last_data_time > 3000))
    {
      ESP_LOGI(TAG, "End of transmission. Total decrypted: %zu bytes", total_received);
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(1)); // Небольшая задержка для Watchdog
  }

  // 6. Освобождение памяти
  mbedtls_aes_free(&aes_ctx);
  free(rx_buf);
  free(dec_buf);
  free(b64_buf);

  // 7. Завершение OTA
  if (esp_ota_end(update_handle) != ESP_OK)
  {
    ESP_LOGE(TAG, "OTA end failed");
    return;
  }

  // 8. Переключение загрузочного раздела и перезагрузка
  if (esp_ota_set_boot_partition(update_partition) == ESP_OK)
  {
    printf("FIRMWARE_UPDATE_SUCCESS\n");
    fflush(stdout);                  // Принудительно отправляем данные в USB
    vTaskDelay(pdMS_TO_TICKS(1000)); // Даем RTOS время на передачу (вместо delay)
    esp_restart();
  }
  else
  {
    ESP_LOGE(TAG, "Failed to set boot partition");
  }
}