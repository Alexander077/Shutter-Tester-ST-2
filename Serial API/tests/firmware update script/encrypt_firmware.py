import argparse
import sys
import os
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# ВАЖНО: Эти ключи должны в точности совпадать с массивами aes_key и aes_iv_init в C++ коде ESP32!
AES_KEY = bytes([
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
])

AES_IV = bytes([
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
])

def encrypt_file(input_file, output_file):
    if not os.path.exists(input_file):
        print(f"Ошибка: Исходный файл '{input_file}' не найден!")
        sys.exit(1)

    try:
        with open(input_file, 'rb') as f:
            firmware_data = f.read()
            
        original_size = len(firmware_data)

        # AES-CBC требует, чтобы размер файла был кратен 16 байтам.
        # pad() добавляет необходимые байты в конец.
        padded_data = pad(firmware_data, AES.block_size)

        cipher = AES.new(AES_KEY, AES.MODE_CBC, AES_IV)
        encrypted_data = cipher.encrypt(padded_data)

        with open(output_file, 'wb') as f:
            f.write(encrypted_data)
            
        print("-" * 40)
        print("Успешное шифрование (AES-256-CBC)")
        print(f"Входной файл:   {input_file} ({original_size} байт)")
        print(f"Выходной файл:  {output_file} ({len(encrypted_data)} байт)")
        print("-" * 40)

    except Exception as e:
        print(f"Произошла ошибка при шифровании: {e}")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(
        description="Утилита для шифрования прошивки ESP32 (AES-256-CBC)",
        epilog="Пример использования: python encrypt_firmware.py firmware.bin firmware_encrypted.bin"
    )
    
    parser.add_argument(
        "input", 
        help="Путь к оригинальному незашифрованному файлу прошивки (.bin)"
    )
    parser.add_argument(
        "output", 
        nargs="?", 
        help="Путь для сохранения зашифрованного файла. Если не указан, добавится суффикс _encrypted"
    )

    args = parser.parse_args()

    # Если выходной файл не указан, генерируем имя автоматически
    output_file = args.output
    if not output_file:
        base, ext = os.path.splitext(args.input)
        output_file = f"{base}_encrypted{ext}"

    encrypt_file(args.input, output_file)

if __name__ == "__main__":
    main()