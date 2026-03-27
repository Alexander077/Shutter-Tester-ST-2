import serial
import json
import time
import os
import sys
import base64
from typing import Optional, Dict, Any, Tuple

# Cross-platform keyboard handling
if sys.platform == 'win32':
    import msvcrt
else:
    import termios
    import tty
    import select

# ============================================================================
# CONFIGURATION
# ============================================================================

PORT = 'COM40'
BAUD_RATE = 115200

# Sensor types (indices for API)
SENSOR_TYPES = [
    (0, "35mm (36x24mm)"),
    (1, "6x4.5 (60x45mm)"),
    (2, "6x6 (60x60mm)"),
    (3, "6x7 (70x60mm)"),
]

# Curtain movement types
CURTAIN_MOVEMENTS = [
    (0, "Horizontal"),
    (1, "Vertical"),
    (2, "Leaf"),
]


# ============================================================================
# SERIAL COMMUNICATION
# ============================================================================

def send_json_command(ser: serial.Serial, payload: Dict[str, Any], timeout: float = 10.0) -> Optional[Dict]:
    """Send JSON command and wait for JSON response, ignoring system logs."""

    msg = json.dumps(payload) + '\n'
    print(f"\n[>>>] SEND: {msg.strip()}")

    payload_bytes = msg.encode('utf-8')

    # Send in chunks to avoid USB buffer overflow
    for i in range(0, len(payload_bytes), 32):
        ser.write(payload_bytes[i:i+32])
        ser.flush()
        time.sleep(0.02)

    # Wait for response
    end_time = time.time() + timeout
    while time.time() < end_time:
        if ser.in_waiting > 0:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                continue

            if line.startswith('{') and line.endswith('}'):
                print(f"[<<<] RESPONSE: {line}")
                time.sleep(0.1)
                return json.loads(line)
            else:
                print(f"[LOG] {line}")

        time.sleep(0.01)

    print("[!!!] TIMEOUT: No response from device")
    return None


def send_raw_command(ser: serial.Serial, cmd: str):
    """Send raw string command."""
    cmd_bytes = cmd.encode('utf-8')
    for i in range(0, len(cmd_bytes), 32):
        ser.write(cmd_bytes[i:i+32])
        ser.flush()
        time.sleep(0.02)


def connect_to_device() -> Optional[serial.Serial]:
    """Connect to the device and initialize."""
    try:
        ser = serial.Serial(PORT, BAUD_RATE, timeout=0.1)
        ser.setDTR(False)
        ser.setRTS(False)

        print(f"Connected to {PORT} at {BAUD_RATE} baud")
        print("Waiting for device initialization (1 sec)...")
        time.sleep(1)

        # Clear buffer from startup logs
        ser.reset_input_buffer()

        return ser
    except serial.SerialException as e:
        print(f"[ERROR] Cannot open COM port: {e}")
        print("Check that the correct PORT is specified and no other program is using it.")
        return None


# ============================================================================
# KEYBOARD INPUT (Cross-platform)
# ============================================================================

def kbhit() -> bool:
    """Check if a key has been pressed (cross-platform)."""
    if sys.platform == 'win32':
        return msvcrt.kbhit()
    else:
        return select.select([sys.stdin], [], [], 0)[0]


def getch() -> str:
    """Get a single character from keyboard (cross-platform)."""
    if sys.platform == 'win32':
        ch = msvcrt.getch()
        if ch in (b'\xe0', b'\x00'):  # Arrow keys prefix
            ch = msvcrt.getch()
        return ch.decode('utf-8', errors='ignore')
    else:
        return sys.stdin.read(1)


def is_escape_pressed() -> bool:
    """Check if ESC key is pressed. Returns True if ESC was pressed."""
    if kbhit():
        ch = getch()
        if ch == '\x1b' or ch == 'q' or ch == 'Q':
            return True
    return False


class TerminalContext:
    """Context manager for terminal settings (Unix only)."""
    def __init__(self):
        self.old_settings = None
        self.is_unix = sys.platform != 'win32'

    def __enter__(self):
        if self.is_unix:
            self.old_settings = termios.tcgetattr(sys.stdin)
            tty.setcbreak(sys.stdin.fileno())
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if self.is_unix and self.old_settings:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self.old_settings)
        return False


# ============================================================================
# MENU DISPLAY
# ============================================================================

def clear_screen():
    """Clear the terminal screen."""
    os.system('cls' if os.name == 'nt' else 'clear')


def print_header(title: str):
    """Print a formatted header."""
    print("\n" + "=" * 60)
    print(f"  {title}")
    print("=" * 60)


def print_menu():
    """Print the main menu."""
    clear_screen()
    print_header("ESP32-S2 Shutter Tester API Tester")
    print()
    print("  1. Light Setup Mode")
    print("  2. Shutter Speed Measurement Mode")
    print("  3. Records Storage Operations")
    print("  4. Firmware Update")
    print("  5. Exit")
    print()
    print("-" * 60)
    print("Press 'q' or ESC in modes with real-time data to exit")


# ============================================================================
# LIGHT SETUP MODE
# ============================================================================

def light_setup_mode(ser: serial.Serial):
    """Test Light Setup API - real-time JSON status display."""
    print_header("Light Setup Mode")
    print("Starting light setup mode...")
    print("Device will send light status JSON in real-time.")
    print("Press 'q' or ESC to return to main menu.")
    print("-" * 60)

    # Send command to enter light setup mode
    payload = {"cmd": "API_REQUEST_LIGHT_SETUP"}
    msg = json.dumps(payload) + '\n'
    print(f"Sending: {msg.strip()}")
    send_raw_command(ser, msg)

    print("\nWaiting for light status data...")
    print("(Data will be displayed in overwrite mode)")
    print()

    with TerminalContext():
        last_json_lines = 0
        while True:
            # Check for ESC/q key
            if is_escape_pressed():
                print("\n[EXIT] Exiting light setup mode...")
                return

            # Read data from serial
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                if not line:
                    continue

                if line.startswith('{') and line.endswith('}'):
                    try:
                        data = json.loads(line)
                        if data.get("cmd") == "API_REQUEST_LIGHT_SETUP":
                            # Move cursor up and overwrite previous lines
                            if last_json_lines > 0:
                                print(f"\033[{last_json_lines}A", end='')
                            print(f"\033[K", end='')  # Clear to end of line
                            print(json.dumps(data, indent=2))
                            # Count lines for next overwrite
                            json_str = json.dumps(data, indent=2)
                            last_json_lines = json_str.count('\n') + 1
                        else:
                            print(f"\n[JSON] {line}")
                    except json.JSONDecodeError:
                        print(f"\n[JSON] {line}")
                else:
                    print(f"\r[LOG] {line}", end='', flush=True)

            time.sleep(0.01)


# ============================================================================
# MEASUREMENT MODE
# ============================================================================

def select_sensor_type() -> int:
    """Prompt user to select sensor type."""
    clear_screen()
    print_header("Measurement Mode - Select Sensor Type")
    print()
    for idx, (code, name) in enumerate(SENSOR_TYPES):
        print(f"  {idx + 1}. {name}")
    print()
    print("-" * 60)

    while True:
        try:
            choice = input("Select sensor type (1-4): ").strip()
            choice_num = int(choice)
            if 1 <= choice_num <= 4:
                return choice_num - 1
            print("Invalid choice. Please enter 1-4.")
        except ValueError:
            print("Invalid input. Please enter a number.")


def select_curtain_movement() -> int:
    """Prompt user to select curtain movement."""
    clear_screen()
    print_header("Measurement Mode - Select Curtain Movement")
    print()
    for idx, (code, name) in enumerate(CURTAIN_MOVEMENTS):
        print(f"  {idx + 1}. {name}")
    print()
    print("-" * 60)

    while True:
        try:
            choice = input("Select curtain movement (1-3): ").strip()
            choice_num = int(choice)
            if 1 <= choice_num <= 3:
                return choice_num - 1
            print("Invalid choice. Please enter 1-3.")
        except ValueError:
            print("Invalid input. Please enter a number.")


def measurement_mode(ser: serial.Serial):
    """Test Shutter Speed Measurement API."""
    # Select sensor type
    sensor_index = select_sensor_type()

    # Select curtain movement
    curtain_movement = select_curtain_movement()

    clear_screen()
    print_header("Measurement Mode - Ready")
    print()
    print(f"Selected sensor: {SENSOR_TYPES[sensor_index][1]}")
    print(f"Curtain movement: {CURTAIN_MOVEMENTS[curtain_movement][1]}")
    print()
    print("Starting measurement...")
    print("Press 'q' or ESC to cancel and return to main menu.")
    print("-" * 60)

    # Send measurement command
    payload = {
        "cmd": "API_REQUEST_MEASURE",
        "sensorIndex": sensor_index,
        "curtainMovement": curtain_movement
    }
    msg = json.dumps(payload) + '\n'
    print(f"Sending: {msg.strip()}")
    send_raw_command(ser, msg)

    print("\nWaiting for measurement results...")
    print("(This may take some time...)")
    print()

    with TerminalContext():
        while True:
            # Check for ESC/q key
            if is_escape_pressed():
                print("\n[EXIT] Exiting measurement mode...")
                return

            # Read data from serial
            if ser.in_waiting > 0:
                line = ser.readline().decode('utf-8', errors='ignore').strip()

                if not line:
                    continue

                if line.startswith('{') and line.endswith('}'):
                    try:
                        data = json.loads(line)
                        if data.get("cmd") == "API_REQUEST_MEASURE":
                            print("\n" + "-" * 60)
                            print("[RESULT] Measurement completed!")
                            print("-" * 60)
                            print(json.dumps(data, indent=2))
                            print("-" * 60)
                            print("\nPress 'q' or ESC to return to main menu...")

                            # Wait for exit command
                            while True:
                                if is_escape_pressed():
                                    return
                                time.sleep(0.05)
                    except json.JSONDecodeError:
                        print(f"\n[JSON] {line}")
                else:
                    print(f"\r[LOG] {line}", end='', flush=True)

            time.sleep(0.01)


# ============================================================================
# RECORDS STORAGE MODE
# ============================================================================

def records_storage_mode(ser: serial.Serial):
    """Test Records Storage API - run all CRUD operations."""
    print_header("Records Storage Operations")
    print("Running all CRUD operations automatically...")
    print("-" * 60)

    # TEST 1: Get list of records
    print("\n=== TEST 1: GET RECORDS LIST ===")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORDS_LIST"})
    time.sleep(1)

    # TEST 2: Create new record
    print("\n=== TEST 2: CREATE NEW RECORD ===")
    new_record_data = {
        "recordNumber": 0,  # 0 means create new
        "sensor0Time": 1.25,
        "sensor1Time": 1.30,
        "curtain1spanAspeed": 15.5,
        "curtain1spanAtime": 2.1,
        "slitWidthAverage": 3.14
    }

    res = send_json_command(ser, {"cmd": "API_REQUEST_SAVE_RECORD", "record": new_record_data})

    created_id = None
    if res and res.get("status") == "API_RESPONSE_STATUS_OK":
        created_id = res.get("recordNumber")
        print(f"---> Success! Created record with ID: {created_id}")
    else:
        print("---> Error creating record. Stopping tests.")
        input("\nPress Enter to continue...")
        return

    time.sleep(1)

    # TEST 3: Read the newly created record
    print(f"\n=== TEST 3: READ RECORD {created_id} ===")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORD", "recordNumber": created_id})
    time.sleep(1)

    # TEST 4: Update record
    print(f"\n=== TEST 4: UPDATE RECORD {created_id} ===")
    update_record_data = new_record_data.copy()
    update_record_data["recordNumber"] = created_id
    update_record_data["sensor0Time"] = 99.99
    update_record_data["slitWidthAverage"] = 5.55

    send_json_command(ser, {"cmd": "API_REQUEST_SAVE_RECORD", "record": update_record_data})
    time.sleep(1)

    # TEST 5: Verify update
    print(f"\n=== TEST 5: VERIFY UPDATED DATA ===")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORD", "recordNumber": created_id})
    time.sleep(1)

    # TEST 6: Delete record
    print(f"\n=== TEST 6: DELETE RECORD {created_id} ===")
    send_json_command(ser, {"cmd": "API_REQUEST_DELETE_RECORD", "recordNumber": created_id})
    time.sleep(1)

    # TEST 7: Verify deletion (record should not be found)
    print(f"\n=== TEST 7: VERIFY DELETION ===")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORD", "recordNumber": created_id})
    time.sleep(1)

    # Final list view
    print("\n=== TEST 8: FINAL RECORDS LIST ===")
    send_json_command(ser, {"cmd": "API_REQUEST_GET_RECORDS_LIST"})

    print("\n" + "-" * 60)
    print("[COMPLETE] All records storage tests completed successfully!")
    print("-" * 60)

    input("\nPress Enter to continue...")


# ============================================================================
# FIRMWARE UPDATE MODE
# ============================================================================

def firmware_update_mode(ser: serial.Serial):
    """Test Firmware Update API."""
    clear_screen()
    print_header("Firmware Update Mode")
    print()

    # Get file path
    file_path = input("Enter path to encrypted .bin file (or drag & drop): ").strip()
    file_path = file_path.strip('"\'')

    if not os.path.exists(file_path):
        print(f"[ERROR] File not found: {file_path}")
        input("\nPress Enter to continue...")
        return

    print(f"\nPreparing for update...")
    print(f"File: {file_path}")
    print(f"Size: {os.path.getsize(file_path)} bytes")
    print(f"Port: {PORT} | Baud: {BAUD_RATE}")
    print()

    chunk_size = 48
    file_size = os.path.getsize(file_path)

    # Send OTA start command
    print("Sending firmware update command...")
    start_cmd = json.dumps({"cmd": "API_REQUEST_FIRMWARE_UPDATE"}) + '\n'
    ser.write(start_cmd.encode('ascii'))
    ser.flush()

    print("Waiting for FIRMWARE_UPDATE_READY signal from device...")

    # Wait for ready signal
    is_ready = False
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
                            print(f"\n[ERROR] Device error: {resp.get('message', 'Unknown error')}")
                            input("\nPress Enter to continue...")
                            return
                except json.JSONDecodeError:
                    print(f"[Device]: {line}")
        time.sleep(0.01)

    if not is_ready:
        print("\n[ERROR] Timeout waiting for ready signal.")
        input("\nPress Enter to continue...")
        return

    print("\n[SYSTEM] Ready signal received! Starting data transfer.")
    print("WARNING: Do NOT disconnect the device during update...")

    sent_bytes = 0
    start_time = time.time()
    ack_received = False

    time.sleep(0.1)

    # Main data transfer loop
    with open(file_path, 'rb') as f:
        while True:
            chunk = f.read(chunk_size)
            if not chunk:
                break

            # Encode to Base64
            b64_data = base64.b64encode(chunk).decode('ascii') + '\n'
            ser.write(b64_data.encode('ascii'))
            ser.flush()

            sent_bytes += len(chunk)

            # Wait for ACK
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
                                    sys.stdout.write(f"\rProgress: [{ack_bytes}/{file_size} bytes] {progress:.1f}% ")
                                    sys.stdout.flush()

                                    if ack_bytes >= sent_bytes:
                                        ack_received = True
                                        break

                                elif status == "API_RESPONSE_FIRMWARE_UPDATE_FAILED":
                                    print(f"\n\n[ERROR] Write error: {resp.get('message', 'Flash write failed')}")
                                    input("\nPress Enter to continue...")
                                    return

                        except json.JSONDecodeError:
                            pass
                else:
                    time.sleep(0.005)

            if not ack_received:
                print(f"\n\n[ERROR] Timeout: Device did not acknowledge {sent_bytes} bytes!")
                break

    # Final status check
    if ack_received:
        total_time = time.time() - start_time
        print(f"\n\nFile transfer completed in {total_time:.1f} sec.")
        print("Waiting for FIRMWARE_UPDATE_SUCCESS status...")

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
                                print("\n[SUCCESS] Firmware uploaded successfully!")
                                print("You need to reboot the device to complete the update.")
                                input("\nPress Enter to continue...")
                                return
                            elif status == "API_RESPONSE_FIRMWARE_UPDATE_FAILED":
                                print(f"\n[ERROR] Final stage error: {resp.get('message')}")
                                input("\nPress Enter to continue...")
                                return
                    except json.JSONDecodeError:
                        print(f"[Device]: {line}")
            time.sleep(0.01)

        print("\n[WARNING] SUCCESS status not received, but data was sent.")
        print("Check the device for update status.")

    input("\nPress Enter to continue...")


# ============================================================================
# MAIN
# ============================================================================

def main():
    """Main entry point."""
    clear_screen()
    print_header("Shutter Tester ST-2 Serial API Demo")

    # Connect to device
    ser = connect_to_device()
    if ser is None:
        input("\nPress Enter to exit...")
        sys.exit(1)

    try:
        while True:
            print_menu()
            choice = input("Select option (1-5): ").strip()

            if choice == '1':
                light_setup_mode(ser)
            elif choice == '2':
                measurement_mode(ser)
            elif choice == '3':
                records_storage_mode(ser)
            elif choice == '4':
                firmware_update_mode(ser)
            elif choice == '5':
                print("\nExiting...")
                break
            else:
                print("\nInvalid choice. Please enter 1-5.")
                time.sleep(1)

    except KeyboardInterrupt:
        print("\n\nInterrupted by user.")
    finally:
        ser.close()
        print("\nSerial port closed.")


if __name__ == "__main__":
    main()
