import serial
import json
import time

# --- НАСТРОЙКИ ---
# Укажите ваш COM-порт.
# Для Windows это обычно 'COM3', 'COM4' и т.д.
# Для Mac/Linux это обычно '/dev/ttyUSB0' или '/dev/cu.SLAB_USBtoUART'
PORT = 'COM40' 
BAUD_RATE = 115200

def send_and_receive(ser, payload, timeout=10.0):
    """Отправляет JSON и ждет ответный JSON, игнорируя системные логи ESP32"""
    
    # Формируем строку и добавляем символ переноса строки \n
    msg = json.dumps(payload) + '\n'
    print(f"\n[>>>] ОТПРАВКА: {msg.strip()}")
    
    payload_bytes = msg.encode('utf-8')
    
    # ОТПРАВКА ЧАНКАМИ (кусочками по 32 байта)
    # Это спасает аппаратный USB-буфер ESP32 (64 байта) от переполнения
    for i in range(0, len(payload_bytes), 32):
        ser.write(payload_bytes[i:i+32])
        ser.flush()
        time.sleep(0.02) # Даем ESP32 20 мс, чтобы "проглотить" этот кусочек
        
    # Ждем ответа
    end_time = time.time() + timeout
    while time.time() < end_time:
        if ser.in_waiting > 0:
            # Читаем строку
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue
            
            # Если строка похожа на JSON-ответ
            if line.startswith('{') and line.endswith('}'):
                print(f"[<<<] ОТВЕТ:  {line}")
                
                # ПАУЗА ПОСЛЕ ОТВЕТА (Критически важно!)
                # Даем ESP32 100 мс на то, чтобы очистить память (cJSON_Delete)
                # и вернуться к fgetc() до того, как полетит следующий тест.
                time.sleep(0.1) 
                
                return json.loads(line)
            else:
                # Печатаем логи (ESP_LOG или printf)
                print(f"[LOG] {line}")
                
        time.sleep(0.01)

    print("[!!!] ТАЙМАУТ: Устройство не ответило")
    return None


def run_tests():
    try:
        # Открываем порт. setDTR(False) помогает избежать случайной перезагрузки некоторых ESP плат
        ser = serial.Serial(PORT, BAUD_RATE, timeout=0.1)
        ser.setDTR(False)
        ser.setRTS(False)
        
        print(f"Подключено к {PORT} на скорости {BAUD_RATE}")
        print("Ожидание инициализации платы (2 сек)...")
        time.sleep(2)
        
        # Очищаем буфер от стартовых логов
        ser.reset_input_buffer()

        # ==========================================
        # ТЕСТ 1: Получение списка записей
        # ==========================================
        print("\n=== ТЕСТ 1: ЧТЕНИЕ СПИСКА ЗАПИСЕЙ ===")
        send_and_receive(ser, {"cmd": "API_REQUEST_GET_RECORDS_LIST"})

        time.sleep(1) 

        # ==========================================
        # ТЕСТ 2: Создание новой записи
        # ==========================================
        print("\n=== ТЕСТ 2: СОЗДАНИЕ НОВОЙ ЗАПИСИ ===")
        new_record_data = {
            "recordNumber": 0, # 0 означает создание новой
            "sensor0Time": 1.25,
            "sensor1Time": 1.30,
            "curtain1spanAspeed": 15.5,
            "curtain1spanAtime": 2.1,
            "slitWidthAverage": 3.14
        }
        
        res = send_and_receive(ser, {"cmd": "API_REQUEST_SAVE_RECORD", "record": new_record_data})
        
        created_id = None
        if res and res.get("status") == "API_RESPONSE_STATUS_OK":
            created_id = res.get("recordNumber")
            print(f"---> Успех! Создана запись с ID: {created_id}")
        else:
            print("---> Ошибка создания записи. Прерывание тестов.")
            return

        time.sleep(1)

        # ==========================================
        # ТЕСТ 3: Чтение только что созданной записи
        # ==========================================
        print(f"\n=== ТЕСТ 3: ЧТЕНИЕ ЗАПИСИ {created_id} ===")
        send_and_receive(ser, {"cmd": "API_REQUEST_GET_RECORD", "recordNumber": created_id})
        
        time.sleep(1)

        # ==========================================
        # ТЕСТ 4: Обновление записи
        # ==========================================
        print(f"\n=== ТЕСТ 4: ОБНОВЛЕНИЕ ЗАПИСИ {created_id} ===")
        # Меняем пару значений для проверки
        update_record_data = new_record_data.copy()
        update_record_data["recordNumber"] = created_id # Указываем существующий ID
        update_record_data["sensor0Time"] = 99.99
        update_record_data["slitWidthAverage"] = 5.55
        
        send_and_receive(ser, {"cmd": "API_REQUEST_SAVE_RECORD", "record": update_record_data})

        time.sleep(1)

        # ==========================================
        # ТЕСТ 5: Проверка обновления
        # ==========================================
        print(f"\n=== ТЕСТ 5: ПРОВЕРКА ОБНОВЛЕННЫХ ДАННЫХ ===")
        send_and_receive(ser, {"cmd": "API_REQUEST_GET_RECORD", "recordNumber": created_id})

        time.sleep(1)

        # ==========================================
        # ТЕСТ 6: Удаление записи
        # ==========================================
        print(f"\n=== ТЕСТ 6: УДАЛЕНИЕ ЗАПИСИ {created_id} ===")
        send_and_receive(ser, {"cmd": "API_REQUEST_DELETE_RECORD", "recordNumber": created_id})

        time.sleep(1)

        # ==========================================
        # ТЕСТ 7: Проверка удаления (запись не должна быть найдена)
        # ==========================================
        print(f"\n=== ТЕСТ 7: ПРОВЕРКА УДАЛЕНИЯ ===")
        send_and_receive(ser, {"cmd": "API_REQUEST_GET_RECORD", "recordNumber": created_id})
        
        time.sleep(1)

        # Финальный просмотр списка (убедиться, что ID пропал)
        print("\n=== ТЕСТ 8: ФИНАЛЬНЫЙ СПИСОК ЗАПИСЕЙ ===")
        send_and_receive(ser, {"cmd": "API_REQUEST_GET_RECORDS_LIST"})

        ser.close()
        print("\nТестирование завершено. Порт закрыт.")

    except serial.SerialException as e:
        print(f"\n[ОШИБКА] Не удалось открыть COM-порт: {e}")
        print("Проверьте, что указан правильный PORT и закройте Serial Monitor в VS Code.")

if __name__ == "__main__":
    run_tests()