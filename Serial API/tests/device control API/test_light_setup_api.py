import serial
import json
import time
import sys


SERIAL_PORT = 'COM40' 
BAUD_RATE = 115200

def main():
    print(f"Подключение к {SERIAL_PORT} на скорости {BAUD_RATE}...")
    
    try:
        # Открываем Serial порт
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    except serial.SerialException as e:
        print(f"Ошибка подключения: {e}")
        print("Проверь, правильный ли COM-порт и не занят ли он другой программой (например, Serial Monitor в VS Code).")
        sys.exit(1)

    # ESP32 часто перезагружается при открытии Serial-соединения (из-за DTR/RTS)
    # Ждем пару секунд, пока плата загрузится
    time.sleep(2)
    
    # Очищаем буфер от стартового мусора (логи загрузки ESP-IDF)
    ser.reset_input_buffer()

    # Формируем команду перехода в режим настройки света
    payload = {"cmd": "API_REQUEST_LIGHT_SETUP"}
    cmd_str = json.dumps(payload) + "\n"
    
    print(f"Отправка команды: {cmd_str.strip()}")
    ser.write(cmd_str.encode('utf-8'))
    
    print("Ожидание данных (нажми Ctrl+C для выхода)...\n")
    print("-" * 40)

    try:
        while True:
            # Читаем строку до символа \n
            line = ser.readline().decode('utf-8', errors='replace').strip()
            
            if line:
                try:
                    # Пытаемся распарсить строку как JSON
                    data = json.loads(line)
                    
                    # Если это наш статус света, выводим красиво
                    if data.get("cmd") == "API_REQUEST_LIGHT_SETUP":
                        print(f"Качество: {data.get('lightQuality'):<7} | "
                              f"Сенсор 1: {data.get('sensor1Level'):<4} ({data.get('sensor1Status')}) | "
                              f"Сенсор 2: {data.get('sensor2Level'):<4} ({data.get('sensor2Status')})")
                    else:
                        # Если пришел какой-то другой JSON
                        print("JSON:", data)
                        
                except json.JSONDecodeError:
                    # Если это не JSON (например, логи ESP_LOGI), просто выводим как текст
                    print(f"LOG: {line}")
                    
    except KeyboardInterrupt:
        print("\nОстановка скрипта пользователем.")
    finally:
        ser.close()
        print("Serial порт закрыт.")

if __name__ == "__main__":
    main()