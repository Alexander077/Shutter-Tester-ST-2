import serial
import time
import os
import sys
import base64
import json

def main():
    print("=== Интерактивный скрипт OTA-обновления ESP32-S2 (JSON API) ===")
    
    # 1. Запрашиваем COM-порт
    # port = input("Введите COM-порт (например, COM3 или /dev/ttyACM0): ").strip()
    port = "COM40"
    if not port:
        print("[ОШИБКА] Порт не может быть пустым.")
        return

    # 2. Запрашиваем путь к файлу
    # file_path = input("Введите путь к зашифрованному файлу .bin (можно перетащить файл в окно): ").strip()
    file_path = "firmware_encrypted.bin"
    
    # Убираем кавычки (от drag & drop)
    file_path = file_path.strip('"\'') 

    if not os.path.exists(file_path):
        print(f"[ОШИБКА] Файл '{file_path}' не найден. Проверьте путь.")
        return

    baudrate = 115200
    send_ota_file(port, baudrate, file_path)

def send_ota_file(port, baudrate, file_path):
    chunk_size = 48 # Чанк в 48 байт -> 64 байта в Base64
    file_size = os.path.getsize(file_path)
    
    print(f"\nПодготовка к обновлению...")
    print(f"Файл: {file_path}")
    print(f"Размер: {file_size} байт")
    print(f"Порт: {port} | Скорость: {baudrate} бод\n")

    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        time.sleep(2) # Даем время на инициализацию native USB

        # Очищаем буфер от логов загрузки перед началом работы
        ser.reset_input_buffer()

        # --- НОВЫЙ ШАГ: Инициируем обновление через основное API ---
        print("Отправка команды на запуск режима OTA (API_REQUEST_FIRMWARE_UPDATE)...")
        start_cmd = json.dumps({"cmd": "API_REQUEST_FIRMWARE_UPDATE"}) + '\n'
        ser.write(start_cmd.encode('ascii'))
        ser.flush()

        print("Ожидание сигнала FIRMWARE_UPDATE_READY от устройства...")
        is_ready = False
        
        # Ждем готовности (таймаут 10 секунд)
        timeout_ready = time.time() + 10.0
        while not is_ready and time.time() < timeout_ready:
            if ser.in_waiting > 0:
                line = ser.readline().decode(errors='ignore').strip()
                if line:
                    try:
                        resp = json.loads(line)
                        if resp.get("cmd") == "API_REQUEST_FIRMWARE_UPDATE":
                            status = resp.get("status")
                            if status == "API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA":
                                is_ready = True
                            elif status == "API_RESPONSE_STATUS_ERROR":
                                print(f"\n[ОШИБКА УСТРОЙСТВА] {resp.get('message', 'Неизвестная ошибка инициализации')}")
                                return
                    except json.JSONDecodeError:
                        # Выводим логи устройства, если это не JSON
                        print(f"[Устройство]: {line}")
            time.sleep(0.01)

        if not is_ready:
            print("\n[ОШИБКА] Таймаут ожидания сигнала готовности. Убедитесь, что устройство обработало команду в SerialAPI.h.")
            return

        print("\n[СИСТЕМА] Сигнал получен! Начинаю отправку данных. Пожалуйста, НЕ ОТКЛЮЧАЙТЕ устройство...")
        
        sent_bytes = 0
        start_time = time.time()
        ack_received = False
        
        time.sleep(0.1)

        # --- Основной цикл отправки Base64 ---
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
                
                # Ожидание ACK от ESP32
                ack_received = False
                chunk_timeout = time.time() + 2 
                
                while time.time() < chunk_timeout:
                    if ser.in_waiting > 0:
                        line = ser.readline().decode(errors='ignore').strip()
                        if line:
                            try:
                                resp = json.loads(line)
                                if resp.get("cmd") == "API_REQUEST_FIRMWARE_UPDATE":
                                    status = resp.get("status")
                                    
                                    if status == "API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK":
                                        ack_bytes = int(resp.get("bytesReceived", 0))
                                        progress = (ack_bytes / file_size) * 100
                                        sys.stdout.write(f"\rПрогресс: [{ack_bytes}/{file_size} байт] {progress:.1f}% ")
                                        sys.stdout.flush()
                                        
                                        if ack_bytes >= sent_bytes:
                                            ack_received = True
                                            break
                                            
                                    elif status == "API_RESPONSE_FIRMWARE_UPDATE_FAILED":
                                        print(f"\n\n[ОШИБКА ЗАПИСИ] {resp.get('message', 'Сбой при записи во флеш')}")
                                        return
                                        
                            except json.JSONDecodeError:
                                pass 
                    else:
                        time.sleep(0.005)
                        
                if not ack_received:
                    print(f"\n\n[ОШИБКА] Таймаут: ESP32 не подтвердила запись {sent_bytes} байт!")
                    break

        # --- Завершение и проверка успеха ---
        if ack_received:
            total_time = time.time() - start_time
            print(f"\n\nОтправка файла завершена за {total_time:.1f} сек.")
            print("Ожидание финального статуса API_RESPONSE_FIRMWARE_UPDATE_SUCCESS и перезагрузки...")
            
            timeout = time.time() + 15.0
            while time.time() < timeout:
                if ser.in_waiting > 0:
                    line = ser.readline().decode(errors='ignore').strip()
                    if line:
                        try:
                            resp = json.loads(line)
                            if resp.get("cmd") == "API_REQUEST_FIRMWARE_UPDATE":
                                status = resp.get("status")
                                if status == "API_RESPONSE_FIRMWARE_UPDATE_SUCCESS":
                                    print("\n[УСПЕХ] Устройство успешно прошито и перезагружается!")
                                    return
                                elif status == "API_RESPONSE_FIRMWARE_UPDATE_FAILED":
                                    print(f"\n[ОШИБКА УСТРОЙСТВА] Ошибка на финальном этапе: {resp.get('message')}")
                                    return
                        except json.JSONDecodeError:
                            print(f"[Устройство]: {line}")
                time.sleep(0.01)
            
            print("\n[ПРЕДУПРЕЖДЕНИЕ] Статус SUCCESS не получен, но данные были отправлены. Проверьте устройство.")

    except serial.SerialException as e:
        print(f"\n[ОШИБКА] Проблема с COM-портом: {e}")
        print("Убедитесь, что порт не занят Serial Монитором (в VS Code / PlatformIO / Arduino IDE).")
    except Exception as e:
        print(f"\n[ОШИБКА] Непредвиденная ошибка: {e}")
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
            print("\nПорт закрыт.")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\nПроцесс прерван пользователем. Порт закрыт.")
        sys.exit(0)