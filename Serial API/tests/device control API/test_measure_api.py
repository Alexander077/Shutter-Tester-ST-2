import serial
import json
import time
import threading
import sys

# === НАСТРОЙКИ ПОДКЛЮЧЕНИЯ ===
PORT = 'COM40'       # Замени на свой порт
BAUDRATE = 115200   # Скорость твоего Serial (обычно 115200 для ESP32)
# =============================

def read_from_port(ser):
    """Фоновая задача для постоянного чтения данных из Serial-порта"""
    while True:
        try:
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                
                # Пытаемся распарсить пришедшую строку как JSON
                try:
                    data = json.loads(line)
                    # Если это наш JSON с результатами измерений
                    if data.get("type") == "measurement_result":
                        print("\n\n" + "="*40)
                        print("🟢 ПОЛУЧЕН РЕЗУЛЬТАТ ИЗМЕРЕНИЯ:")
                        print(json.dumps(data, indent=4, ensure_ascii=False))
                        print("="*40 + "\n")
                        print("Ваш выбор: ", end='', flush=True) # Возвращаем промпт меню
                    else:
                        # Какой-то другой JSON
                        print(f"\n[ESP32 JSON]: {json.dumps(data, ensure_ascii=False)}")
                        print("Ваш выбор: ", end='', flush=True)
                        
                except json.JSONDecodeError:
                    # Это обычный текстовый лог ESP_LOGI / ESP_LOGE
                    print(f"\n[ESP32 LOG]: {line}")
                    print("Ваш выбор: ", end='', flush=True)
                    
        except serial.SerialException:
            print("\n[ОШИБКА]: Соединение разорвано.")
            break
        time.sleep(0.01)

def send_command(ser, cmd_dict):
    """Отправка словаря в виде JSON строки с символом переноса строки"""
    # Сериализуем словарь в строку и обязательно добавляем \n в конце, 
    # чтобы ESP32 поняла, что команда завершена
    json_str = json.dumps(cmd_dict) + '\n'
    ser.write(json_str.encode('utf-8'))
    print(f"\n[ПК TX] ---> {json_str.strip()}")
    time.sleep(0.1) # Небольшая пауза после отправки

def main():
    try:
        ser = serial.Serial(PORT, BAUDRATE, timeout=1)
        print(f"✅ Успешно подключено к {PORT} на скорости {BAUDRATE}")
    except serial.SerialException as e:
        print(f"❌ Ошибка подключения к {PORT}: {e}")
        print("Проверь порт, кабель и не открыт ли Serial Monitor в VS Code.")
        sys.exit(1)

    # Запускаем чтение порта в отдельном фоновом потоке
    reader_thread = threading.Thread(target=read_from_port, args=(ser,), daemon=True)
    reader_thread.start()

    time.sleep(1) # Даем время на инициализацию соединения

    while True:
        print("\n=== ПАНЕЛЬ УПРАВЛЕНИЯ ТЕСТЕРОМ ===")
        print("1. Режим: Настройка света (light_setup)")
        print("2. Режим: Замер (Узкая пленка 35mm, Горизонтальные шторки)")
        print("3. Режим: Замер (Средний формат, Вертикальные шторки)")
        print("4. Команда: OTA Обновление прошивки (start_aes_serial_ota)")
        print("5. Отправить кастомный JSON")
        print("0. Выход")
        
        choice = input("Ваш выбор: ")
        
        if choice == '1':
            send_command(ser, {"cmd": "light_setup"})
            
        elif choice == '2':
            # Допустим: sensor_index 0 = 35mm, curtain_movement 0 = HORIZONTAL
            send_command(ser, {"cmd": "measure", "sensor_index": 0, "curtain_movement": 0})
            
        elif choice == '3':
            # Допустим: sensor_index 1 = Medium Format, curtain_movement 1 = VERTICAL
            send_command(ser, {"cmd": "measure", "sensor_index": 1, "curtain_movement": 1})
            
        elif choice == '4':
            send_command(ser, {"cmd": "start_aes_serial_ota"})
            
        elif choice == '5':
            custom_input = input("Введи валидный JSON (например: {\"cmd\": \"test\"}): ")
            try:
                # Проверяем валидность перед отправкой
                custom_json = json.loads(custom_input)
                send_command(ser, custom_json)
            except json.JSONDecodeError:
                print("❌ Ошибка: Введен некорректный JSON!")
                
        elif choice == '0':
            print("Закрытие порта и выход...")
            break
        else:
            print("Неизвестная команда.")

    ser.close()

if __name__ == '__main__':
    main()