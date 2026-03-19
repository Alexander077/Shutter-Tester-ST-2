import serial
import time
import os
import argparse
import sys
import base64

def send_ota_file(port, baudrate, file_path):
    chunk_size = 48 # Чанк в 48 байт, который после Base64 будет 64 байта (без учета \n)
    
    if not os.path.exists(file_path):
        print(f"[ОШИБКА] Файл {file_path} не найден.")
        sys.exit(1)

    file_size = os.path.getsize(file_path)
    print(f"=== OTA Обновление ESP32-S2 ===")
    print(f"Файл: {file_path}")
    print(f"Размер: {file_size} байт")
    print(f"Порт: {port} | Скорость: {baudrate} бод")
    print("===============================\n")

    try:
        # Открываем Serial порт
        ser = serial.Serial(port, baudrate, timeout=1)
        # Даем время на инициализацию соединения (важно для native USB)
        time.sleep(2) 
        
        # --- Ожидание сигнала готовности и вывод логов ---
        print("Ожидание сигнала FIRMWARE_UPDATE_READY от ESP32...")
        wait_buffer = ""
        while "FIRMWARE_UPDATE_READY" not in wait_buffer:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting).decode(errors='ignore')
                print(data, end='', flush=True) # Печатаем всё, что приходит
                wait_buffer += data
            time.sleep(0.01)
            
        print("\n\n[СИСТЕМА] Сигнал получен!")
        print("Начинаю отправку данных. Пожалуйста, НЕ ОТКЛЮЧАЙТЕ устройство...")
        
        sent_bytes = 0
        start_time = time.time()
        ack_received = False
        
        with open(file_path, 'rb') as f:
            while True:
                chunk = f.read(chunk_size)
                if not chunk:
                    break
                
                # Кодируем в Base64 и добавляем символ переноса строки
                b64_data = base64.b64encode(chunk).decode('ascii') + '\n'
                ser.write(b64_data.encode('ascii'))
                ser.flush()
                
                sent_bytes += len(chunk)
                
                # Прогресс-бар в консоли
                progress = (sent_bytes / file_size) * 100
                sys.stdout.write(f"\rПрогресс: [{sent_bytes}/{file_size} байт] {progress:.1f}% ")
                sys.stdout.flush()
                
                # Ожидание ACK от ESP32
                ack_received = False
                chunk_timeout = time.time() + 5.0 # Ждем максимум 5 секунд на один чанк
                
                while time.time() < chunk_timeout:
                    if ser.in_waiting > 0:
                        # Читаем строку
                        line = ser.readline().decode(errors='ignore').strip()
                        
                        if line.startswith("ACK:"):
                            # Для отладки (раскомментируйте, если нужно видеть каждый шаг)
                            sys.stdout.write(f" [{line}] ")
                            sys.stdout.flush()
                            
                            try:
                                ack_bytes = int(line.split(":")[1])
                                if ack_bytes >= sent_bytes:
                                    ack_received = True
                                    break
                            except ValueError:
                                pass
                        elif line:
                            # Печатаем логи с ESP32 (например, ESP_LOGE), не затирая прогресс-бар
                            print(f"\n[ESP32]: {line}")
                    else:
                        time.sleep(0.005)
                        
                if not ack_received:
                    print(f"\n\n[ОШИБКА] Таймаут: ESP32 не подтвердила запись {sent_bytes} байт!")
                    break
                
        if ack_received:
            total_time = time.time() - start_time
            print(f"\n\nОтправка файла завершена за {total_time:.1f} сек.")
            print("Ожидание финального ответа от устройства (перезагрузка)...")
            
            # Ждем 15 секунд (вместо 5) для успешного окончания валидации прошивки и перезагрузки
            timeout = time.time() + 15
            while time.time() < timeout:
                if ser.in_waiting > 0:
                    response = ser.read(ser.in_waiting)
                    print(response.decode(errors='ignore'), end='', flush=True)
                time.sleep(0.01)
            
    except serial.SerialException as e:
        print(f"\n[ОШИБКА] Проблема с COM-портом: {e}")
        print("Проверьте: не занят ли порт Serial Монитором (например, в VS Code или PlatformIO)?")
    except Exception as e:
        print(f"\n[ОШИБКА] Непредвиденная ошибка: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("\nПорт закрыт.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Скрипт для OTA-обновления зашифрованной прошивки ESP32-S2 по USB (Base64)")
    parser.add_argument("-p", "--port", required=True, help="COM порт (например, COM3 или /dev/ttyACM0)")
    parser.add_argument("-f", "--file", required=True, help="Путь к зашифрованному файлу .bin")
    
    args = parser.parse_args()
    # Вызов функции без передачи chunk_size, так как он теперь жестко задан внутри функции
    send_ota_file(args.port, 115200, args.file)