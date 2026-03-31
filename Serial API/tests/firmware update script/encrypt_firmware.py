import argparse
import sys
import os
from Crypto.Cipher import AES
from Crypto.Util.Padding import pad

# ВАЖНО: Эти ключи должны в точности совпадать с массивами aes_key и aes_iv_init в C++ коде ESP32!
AES_KEY = bytes([
    0x4f, 0x8a, 0x1c, 0xe3, 0x7b, 0x9d, 0x24, 0x55, 
    0xa6, 0xf0, 0x38, 0xcc, 0x19, 0xd2, 0x4e, 0x8f, 
    0xbb, 0x71, 0x05, 0x6a, 0x3d, 0x9c, 0x82, 0xe4, 
    0xf5, 0x09, 0x27, 0x1b, 0xd6, 0xaf, 0x3e, 0x90
])

AES_IV = bytes([
    0x2c, 0x9f, 0x5b, 0x11, 0xd8, 0x4a, 0x73, 0xe6, 
    0x04, 0x8c, 0x39, 0xf2, 0x1d, 0x6e, 0xa5, 0x77
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