import serial
import time
import os
import argparse
import sys

def send_ota_file(port, baudrate, file_path, chunk_size):
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
        
        print("Начинаю отправку данных. Пожалуйста, НЕ ОТКЛЮЧАЙТЕ устройство...")

        with open(file_path, 'rb') as f:
            sent_bytes = 0
            start_time = time.time()

            while True:
                chunk = f.read(chunk_size)
                if not chunk:
                    break
                
                # Отправляем блок данных
                ser.write(chunk)
                ser.flush()
                
                sent_bytes += len(chunk)
                
                # Прогресс-бар в консоли
                progress = (sent_bytes / file_size) * 100
                sys.stdout.write(f"\rПрогресс: [{sent_bytes}/{file_size} байт] {progress:.1f}% ")
                sys.stdout.flush()
                
                # КРИТИЧЕСКИ ВАЖНО: Задержка!
                # ESP32-S2 нужно время на: 1) Чтение из буфера USB 2) Дешифровку AES 3) Запись во Flash
                # Если задержки не будет, буфер USB переполнится и прошивка будет повреждена.
                time.sleep(0.05) 
                
                # Если ESP32 отправляет логи (например, "Block written"), читаем их,
                # чтобы очистить буфер приема ПК и видеть, что происходит.
                while ser.in_waiting > 0:
                    response = ser.read(ser.in_waiting)
                    # Раскомментируйте следующую строку, если хотите видеть логи с ESP32 прямо во время прошивки:
                    # print(f"\n[ESP32]: {response.decode(errors='ignore').strip()}")

        elapsed_time = time.time() - start_time
        print(f"\n\nОтправка файла завершена за {elapsed_time:.1f} сек.")
        print("Ожидание финального ответа от устройства (перезагрузка)...")
        
        # Ждем 5 секунд и выводим всё, что ESP32 скажет напоследок
        timeout = time.time() + 5
        while time.time() < timeout:
            if ser.in_waiting > 0:
                response = ser.read(ser.in_waiting)
                print(response.decode(errors='ignore'), end='')
            time.sleep(0.1)
            
    except serial.SerialException as e:
        print(f"\n[ОШИБКА] Проблема с COM-портом: {e}")
        print("Проверьте: не занят ли порт Serial Монитором (например, в VS Code)?")
    except Exception as e:
        print(f"\n[ОШИБКА] Непредвиденная ошибка: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("\nПорт закрыт.")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Скрипт для OTA-обновления зашифрованной прошивки ESP32-S2 по USB")
    parser.add_argument("-p", "--port", required=True, help="COM порт (например, COM3 или /dev/ttyACM0)")
    parser.add_argument("-f", "--file", required=True, help="Путь к зашифрованному файлу .bin")
    parser.add_argument("-b", "--baud", type=int, default=115200, help="Скорость (по умолчанию 115200)")
    # Размер чанка 1024 байта оптимален для большинства случаев передачи по Serial
    parser.add_argument("-c", "--chunk", type=int, default=1024, help="Размер блока (по умолчанию 1024 байта)")

    args = parser.parse_args()
    send_ota_file(args.port, args.baud, args.file, args.chunk)