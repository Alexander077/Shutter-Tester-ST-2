import serial
import json
import time
import sys
import os
import base64
import ctypes

try:
    import keyboard
except ImportError:
    print("Error: 'keyboard' module not found.")
    print("Please install it using: pip install keyboard")
    sys.exit(1)

# Enable ANSI escape sequences for Windows
if os.name == 'nt':
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)

SERIAL_PORT = 'COM40'
BAUD_RATE = 115200

def clear_screen():
    os.system('cls' if os.name == 'nt' else 'clear')

def send_json_command(ser, payload):
    cmd_str = json.dumps(payload) + "\n"
    ser.write(cmd_str.encode('utf-8'))

def test_light_setup(ser):
    clear_screen()
    print("=== Testing Light Setup API ===")
    print("Press 'Esc' to return to Main Menu.\n")
    
    ser.reset_input_buffer()
    send_json_command(ser, {"cmd": "API_REQUEST_LIGHT_SETUP"})
    
    last_lines_count = 0
    
    while True:
        if keyboard.is_pressed('esc'):
            print("\nExiting Light Setup Mode...")
            time.sleep(0.5)
            break
            
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                try:
                    data = json.loads(line)
                    if data.get("cmd") == "API_REQUEST_LIGHT_SETUP":
                        formatted_json = json.dumps(data, indent=4)
                        lines = formatted_json.split('\n')
                        
                        # Move cursor up by last_lines_count and clear to bottom
                        if last_lines_count > 0:
                            sys.stdout.write(f"\033[{last_lines_count}A")
                            sys.stdout.write("\033[J")
                        
                        print(formatted_json)
                        last_lines_count = len(lines)
                except json.JSONDecodeError:
                    pass
        time.sleep(0.01)

def test_measurement(ser):
    clear_screen()
    print("=== Testing Measurement API ===")
    
    frame_size = input("Enter frame size (e.g., '36x24' or '60x60'): ").strip()
    curtain_dir = input("Enter curtain movement direction (e.g., 'Horizontal', 'Vertical', 'Leaf'): ").strip()
    
    print("\nStarting measurement mode... Waiting for results.")
    print("Press 'Esc' to cancel and return to Main Menu.\n")
    
    ser.reset_input_buffer()
    payload = {
        "cmd": "API_REQUEST_MEASURE",
        "frameSize": frame_size,
        "direction": curtain_dir
    }
    send_json_command(ser, payload)
    
    while True:
        if keyboard.is_pressed('esc'):
            print("\nExiting Measurement Mode...")
            time.sleep(0.5)
            break
            
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                try:
                    data = json.loads(line)
                    if data.get("cmd") == "API_REQUEST_MEASURE":
                        print("\n--- Measurement Result Received ---")
                        print(json.dumps(data, indent=4))
                        print("\nPress 'Esc' to return to Main Menu.")
                except json.JSONDecodeError:
                    # Ignore logs during wait
                    pass
        time.sleep(0.01)

def test_records_api(ser):
    clear_screen()
    print("=== Testing Records Storage API ===")
    print("Running automated sequence...")
    print("Press 'Esc' at any time to return to Main Menu.\n")
    
    ser.reset_input_buffer()
    
    print("1. Requesting Records List...")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORDS_LIST"})
    time.sleep(1)
    
    print("\n2. Requesting Specific Record (Index 0)...")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORD", "index": 0})
    time.sleep(1)
    
    print("\n3. Testing Save Record...")
    send_json_command(ser, {
        "cmd": "API_REQUEST_SAVE_RECORD", 
        "data": {"speed": "1/250", "deviation": "+0.1EV"}
    })
    time.sleep(1)

    print("\nWaiting for device logs/JSONs:")
    print("-" * 30)
    
    while True:
        if keyboard.is_pressed('esc'):
            print("\nExiting Records Test...")
            time.sleep(0.5)
            break
            
        if ser.in_waiting:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                try:
                    data = json.loads(line)
                    print(json.dumps(data, indent=4))
                except json.JSONDecodeError:
                    print(f"LOG: {line}")
        time.sleep(0.01)

def test_firmware_update(ser):
    clear_screen()
    print("=== Testing Firmware Update API ===")
    file_path = input("Enter path to encrypted .bin file: ").strip()
    file_path = file_path.strip('\"\'') 

    if not os.path.exists(file_path):
        print(f"[ERROR] File '{file_path}' not found.")
        time.sleep(2)
        return

    print("Requesting firmware update mode...")
    ser.reset_input_buffer()
    send_json_command(ser, {"cmd": "API_REQUEST_FIRMWARE_UPDATE"})
    
    chunk_size = 48
    try:
        with open(file_path, "rb") as f:
            bin_data = f.read()
            
        total_len = len(bin_data)
        print(f"File loaded. Total size: {total_len} bytes.")
        print("Waiting for device to prepare flash memory...")
        
        offset = 0
        waiting_for_ack = True
        
        while True:
            if keyboard.is_pressed('esc'):
                print("\nFirmware update aborted by user.")
                time.sleep(0.5)
                break
                
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    try:
                        resp = json.loads(line)
                        
                        # ИСПРАВЛЕНИЕ: Устройство присылает ответ в поле "status"
                        status = resp.get("status")
                        cmd = resp.get("cmd")
                        
                        # Определяем фактическое событие
                        actual_event = status if status else cmd
                        
                        if actual_event == "API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA" or actual_event == "API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK":
                            waiting_for_ack = False
                            
                        elif actual_event == "API_RESPONSE_FIRMWARE_UPDATE_SUCCESS":
                            print("\n\n[SUCCESS] Firmware updated successfully. Rebooting...")
                            time.sleep(2)
                            break
                            
                        elif actual_event == "API_RESPONSE_FIRMWARE_UPDATE_FAILED":
                            print(f"\n\n[ERROR] Update failed: {resp.get('message')}")
                            time.sleep(2)
                            break
                            
                        elif actual_event == "API_RESPONSE_STATUS_ERROR":
                            print(f"\n\n[ERROR] Device reported an error: {resp}")
                            time.sleep(2)
                            break
                            
                    except json.JSONDecodeError:
                        # Добавлен \n чтобы не ломать строку прогресс-бара при выводе логов
                        print(f"\nDevice: {line}")
                        
                        # Костыль на случай, если ESP склеит лог и JSON в одну строку в UART
                        if "API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA" in line or "API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK" in line:
                            waiting_for_ack = False
            
            if not waiting_for_ack and offset < total_len:
                chunk = bin_data[offset : offset + chunk_size]
                b64_chunk = base64.b64encode(chunk).decode('utf-8')
                
                payload = {
                    "cmd": "API_RESPONSE_FIRMWARE_UPDATE_CHUNK",
                    "data": b64_chunk,
                    "len": len(chunk),
                    "offset": offset
                }
                
                send_json_command(ser, payload)
                sys.stdout.write(f"\rProgress: {min(offset + chunk_size, total_len)} / {total_len} bytes")
                sys.stdout.flush()
                
                offset += len(chunk)
                waiting_for_ack = True
                
            time.sleep(0.01)
            
    except Exception as e:
        print(f"\n[ERROR] {e}")
        time.sleep(2)

def main():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    except serial.SerialException as e:
        print(f"Connection error on {SERIAL_PORT}: {e}")
        sys.exit(1)
        
    time.sleep(2) # Give ESP32 time to reboot after DTR/RTS

    while True:
        clear_screen()
        print("=========================================")
        print("      ESP32-S2 API TESTING TOOLBOX       ")
        print("=========================================")
        print(" 1. Test Light Setup API")
        print(" 2. Test Measurement API")
        print(" 3. Test Records API")
        print(" 4. Test Firmware Update API")
        print(" 5. Exit")
        print("=========================================")
        
        choice = input("Select an option (1-5): ").strip()
        
        if choice == '1':
            test_light_setup(ser)
        elif choice == '2':
            test_measurement(ser)
        elif choice == '3':
            test_records_api(ser)
        elif choice == '4':
            test_firmware_update(ser)
        elif choice == '5':
            print("Exiting toolbox...")
            break
        else:
            print("Invalid choice. Try again.")
            time.sleep(1)
            
    ser.close()

if __name__ == "__main__":
    # Workaround so keyboard library hook doesn't block the main CLI thread unexpectedly
    try:
        main()
    except KeyboardInterrupt:
        print("\nExiting due to keyboard interrupt.")
        sys.exit(0)