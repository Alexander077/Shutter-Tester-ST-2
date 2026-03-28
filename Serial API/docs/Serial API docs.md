# ESP32-S2 Shutter Tester - Serial API Documentation

This document describes the JSON-based Serial API for the Shutter Tester project. This API allows 3rd-party desktop applications, scripts, and other integrations to interact with the device over a USB CDC connection (COM port).

## Table of Contents
1. [General Information](#general-information)
2. [Light Setup API](#light-setup-api)
3. [Measurement API](#measurement-api)
4. [Records Storage API (CRUD)](#records-storage-api-crud)
5. [Firmware Update API (OTA)](#firmware-update-api-ota)

---

## General Information

### Connection Settings
- **Port:** Varies by OS (e.g., `COM3`, `/dev/ttyACM0`). The ESP32-S2 presents itself as a native USB CDC device.
- **Baud Rate:** `115200` (ignored by native USB CDC, but standard to specify).
- **Data Bits:** 8
- **Parity:** None
- **Stop Bits:** 1

### Protocol Rules
1. **JSON Only:** All commands sent to the device and all structured responses from the device are formatted as JSON objects.
2. **Line Terminator:** Every JSON payload must be single-line and terminated by a newline character `\n`. The device reads characters until it encounters `\n` before parsing the JSON.
3. **Buffering:** To prevent overflowing the hardware buffer, it is recommended to send large commands in chunks (e.g., 32 bytes) with a small delay (e.g., 20ms) between chunks.
4. **Unsolicited Logs:** The ESP-IDF framework might occasionally output standard log messages (e.g., `I (123) TAG: ...`). Your JSON parser should be robust enough to ignore lines that do not start with `{` and end with `}`.
5. **Flow Control:** Neither DTR nor RTS are required for the communication, and setting them to `False` when opening the serial connection can prevent unwanted reboots on some host systems.

---

## Light Setup API

This API is used to put the device into "Light Setup" mode, where it continuously reports the real-time status and quality of the light hitting the sensors.

### Enter Light Setup Mode

**Command:**
```json
{ "cmd": "API_REQUEST_LIGHT_SETUP" }
```

**Continuous Stream Responses:**
Once the device enters this mode, it will continuously stream JSON objects with the current light status.

```json
{
  "cmd": "API_REQUEST_LIGHT_SETUP",
  "sensor1Level": 50,
  "sensor2Level": 45,
  "sensor1Status": "LIGHT_STATUS_OK",
  "sensor2Status": "LIGHT_STATUS_OK",
  "lightQuality": "LIGHT_QUALITY_OK"
}
```

#### Fields:
- `sensor1Level` / `sensor2Level`: An integer percentage (`0` to `100`+) representing the analog light level on the sensor.
- `sensor1Status` / `sensor2Status`: String representing the specific sensor's status. Possible values:
  - `"LIGHT_STATUS_TOO_DIM"`
  - `"LIGHT_STATUS_TOO_BRIGHT"`
  - `"LIGHT_STATUS_OK"`
- `lightQuality`: Overall evaluation of the light setup. Possible values:
  - `"LIGHT_QUALITY_UNKNOWN"`
  - `"LIGHT_QUALITY_OK"`
  - `"LIGHT_QUALITY_BAD"`

---

## Measurement API

Used to configure the device for a measurement, arm it, and retrieve the results once the shutter is released.

### Arm for Measurement

**Command:**
```json
{
  "cmd": "API_REQUEST_MEASURE",
  "sensorIndex": 0,
  "curtainMovement": 0
}
```

#### Configuration Parameters:
- `sensorIndex` (Integer): Defines the frame size of the camera.
  - `0`: 35mm
  - `1`: 6x45
  - `2`: 6x6
  - `3`: 6x7
- `curtainMovement` (Integer): Defines the direction of the shutter movement.
  - `0`: Horizontal
  - `1`: Vertical
  - `2`: Leaf Shutter

**Action:**
Sending this command immediately puts the device into an armed state, waiting for the shutter to fire. There is no immediate response. The response arrives *only* after the shutter has fired and measurements have been collected.

**Measurement Result Response (Success):**
```json
{
  "cmd": "API_REQUEST_MEASURE",
  "status": "API_RESPONSE_STATUS_OK",
  "sensor0Time": 1.25,
  "sensor1Time": 1.30,
  "curtain1spanAtime": 2.1,
  "curtain1spanAspeed": 15.5,
  "curtain2spanAtime": 2.2,
  "curtain2spanAspeed": 15.1,
  "slitWidthSensor0": 3.14,
  "slitWidthSensor1": 3.14,
  "slitWidthAverage": 3.14
}
```
*Note: Depending on the physical result and the selected `curtainMovement`, some numeric fields might be replaced by string error statuses, such as:*
- `"SENSOR_LIGHT_IS_TOO_DIM"`
- `"SENSOR_LIGHT_IS_TOO_BRIGHT"`
- `"SENSOR_TIME_IS_TOO_SHORT"`
- `"BOTH_SENSORS_MEASUREMENTS_REQUIRED"` (e.g., if one sensor failed to trigger)
- `"NOT_AVAILABLE_FOR_LEAF_SHUTERS"` (e.g., curtain speed calculations for leaf shutters)

**Measurement Result Response (Error):**
If the device captures entirely invalid data that cannot be parsed into a measurement:
```json
{
  "cmd": "API_REQUEST_MEASURE",
  "status": "API_RESPONSE_STATUS_ERROR",
  "error": "Invalid measurement data"
}
```

---

## Records Storage API (CRUD)

This set of commands allows you to manage the measurements stored in the device's LittleFS non-volatile memory.

### 1. Get List of Records
Retrieves an array of all valid `recordNumber` IDs stored on the device.

**Command:**
```json
{ "cmd": "API_REQUEST_GET_RECORDS_LIST" }
```

**Response:**
```json
{
  "cmd": "API_REQUEST_GET_RECORDS_LIST",
  "status": "API_RESPONSE_STATUS_OK",
  "records": [1, 2, 3, 5, 8]
}
```

### 2. Read a Single Record
Retrieves the full details of a specific record by its ID.

**Command:**
```json
{
  "cmd": "API_REQUEST_GET_RECORD",
  "recordNumber": 1
}
```

**Response (Found):**
```json
{
  "cmd": "API_REQUEST_GET_RECORD",
  "status": "API_RESPONSE_STATUS_OK",
  "record": {
    "recordNumber": 1,
    "sensor0Time": 1.25,
    "sensor1Time": 1.30,
    "curtain1spanAtime": 2.1,
    "curtain1spanAspeed": 15.5,
    "curtain1TotalTime": 5.5,
    "curtain2spanAspeed": 15.1,
    "curtain2spanAtime": 2.2,
    "curtain2TotalTime": 5.6,
    "slitWidthSensor0": 3.14,
    "slitWidthSensor1": 3.14,
    "slitWidthAverage": 3.14
  }
}
```

**Response (Not Found):**
```json
{
  "cmd": "API_REQUEST_GET_RECORD",
  "status": "API_RESPONSE_STATUS_ERROR",
  "message": "Record not found"
}
```

### 3. Save / Update a Record
Creates a new record or overwrites an existing one. To create a *new* record, pass `"recordNumber": 0`. The device will automatically assign it an available slot and return the newly generated ID. To *update* an existing record, pass its valid `"recordNumber"`.

**Command:**
```json
{
  "cmd": "API_REQUEST_SAVE_RECORD",
  "record": {
    "recordNumber": 0,
    "sensor0Time": 1.25,
    "sensor1Time": 1.30,
    "curtain1spanAspeed": 15.5,
    "curtain1spanAtime": 2.1,
    "slitWidthAverage": 3.14
    // include other numeric fields as necessary...
  }
}
```

**Response (Success):**
```json
{
  "cmd": "API_REQUEST_SAVE_RECORD",
  "status": "API_RESPONSE_STATUS_OK",
  "recordNumber": 5
}
```

**Response (Error):**
```json
{
  "cmd": "API_REQUEST_SAVE_RECORD",
  "status": "API_RESPONSE_STATUS_ERROR",
  "message": "Failed to save record"
}
```

### 4. Delete a Record
Marks a specific record as deleted in the file system.

**Command:**
```json
{
  "cmd": "API_REQUEST_DELETE_RECORD",
  "recordNumber": 1
}
```

**Response (Success):**
```json
{
  "cmd": "API_REQUEST_DELETE_RECORD",
  "status": "API_RESPONSE_STATUS_OK"
}
```

**Response (Not Found / Error):**
```json
{
  "cmd": "API_REQUEST_DELETE_RECORD",
  "status": "API_RESPONSE_STATUS_ERROR",
  "message": "Record not found"
}
```

---

## Firmware Update API (OTA)

Allows updating the device firmware securely over the USB serial connection. The firmware binary must be AES-256 encrypted using the key baked into the existing firmware.

### 1. Initiate Update

**Command:**
```json
{ "cmd": "API_REQUEST_FIRMWARE_UPDATE" }
```

**Response (Ready):**
Wait for the device to initialize the OTA partition and AES context. The device will respond with:
```json
{
  "cmd": "API_REQUEST_FIRMWARE_UPDATE",
  "status": "API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA"
}
```

### 2. Send Firmware Chunks
Once the device is ready, you must read the encrypted `.bin` file, encode it in **Base64**, and send it line by line.

**Rules for transmission:**
1. Read a small chunk of binary data (e.g., `48` bytes).
2. Encode it in Base64.
3. Append a newline character `\n`.
4. Send the string over Serial.

**Wait for Acknowledgment:**
After sending a chunk, **you must wait** for the device to decrypt and write it to flash. The device will reply with an acknowledgment:

```json
{
  "cmd": "API_REQUEST_FIRMWARE_UPDATE",
  "status": "API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK",
  "bytesReceived": 1024
}
```
*Do not send the next chunk until this ACK is received.*

### 3. Completion
Once all bytes have been sent and acknowledged, the device will finalize the OTA process, switch the boot partition, and reply with a success message.

**Response (Success):**
```json
{
  "cmd": "API_REQUEST_FIRMWARE_UPDATE",
  "status": "API_RESPONSE_FIRMWARE_UPDATE_SUCCESS",
  "message": "Firmware update successful."
}
```
*Note: The device will require a restart to boot into the new firmware.*

**Response (Failure at any stage):**
If a failure occurs during initialization, decryption, writing to flash, or finalizing, the device will return:
```json
{
  "cmd": "API_REQUEST_FIRMWARE_UPDATE",
  "status": "API_RESPONSE_FIRMWARE_UPDATE_FAILED",
  "message": "Specific error message here"
}
```
