#pragma once

enum class ApiRequestAction
{
  NO_ACTION,
  GO_TO_LIGHT_SETUP,
  GO_TO_MEASURE
};

volatile ApiRequestAction apiRequestAction = ApiRequestAction::NO_ACTION;

class SerialAPILightStatus
{
public:
  static constexpr const char *LIGHT_STATUS_TOO_DIMM = "LIGHT_STATUS_TOO_DIM";
  static constexpr const char *LIGHT_STATUS_TOO_BRIGHT = "LIGHT_STATUS_TOO_BRIGHT";
  static constexpr const char *LIGHT_STATUS_OK = "LIGHT_STATUS_OK";
};

class SerialAPIRequestAction
{
public:
  static constexpr const char *API_REQUEST_LIGHT_SETUP = "API_REQUEST_LIGHT_SETUP";
  static constexpr const char *API_REQUEST_MEASURE = "API_REQUEST_MEASURE";
  static constexpr const char *API_REQUEST_GET_RECORDS_LIST = "API_REQUEST_GET_RECORDS_LIST";
  static constexpr const char *API_REQUEST_GET_RECORD = "API_REQUEST_GET_RECORD";
  static constexpr const char *API_REQUEST_DELETE_RECORD = "API_REQUEST_DELETE_RECORD";
  static constexpr const char *API_REQUEST_SAVE_RECORD = "API_REQUEST_SAVE_RECORD";
  static constexpr const char *API_REQUEST_FIRMWARE_UPDATE = "API_REQUEST_FIRMWARE_UPDATE";
  static constexpr const char *API_REQUEST_ECHO = "API_REQUEST_ECHO";
};

class SerialAPIResponse
{
public:
  static constexpr const char *API_RESPONSE_STATUS_OK = "API_RESPONSE_STATUS_OK";
  static constexpr const char *API_RESPONSE_STATUS_ERROR = "API_RESPONSE_STATUS_ERROR";
  static constexpr const char *API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA =
      "API_RESPONSE_READY_FOR_FIRMWARE_UPDATE_DATA";
  static constexpr const char *API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK =
      "API_RESPONSE_FIRMWARE_UPDATE_CHUNK_ACK";
  static constexpr const char *API_RESPONSE_FIRMWARE_UPDATE_SUCCESS =
      "API_RESPONSE_FIRMWARE_UPDATE_SUCCESS";
  static constexpr const char *API_RESPONSE_FIRMWARE_UPDATE_FAILED =
      "API_RESPONSE_FIRMWARE_UPDATE_FAILED";
  static constexpr const char *API_RESPONSE_MEASUREMENT_RESULT_DATA =
      "API_RESPONSE_MEASUREMENT_RESULT_DATA";
};

char *serialApiLightQualityStatusesStr[3] = {"LIGHT_QUALITY_UNKNOWN", "LIGHT_QUALITY_OK", "LIGHT_QUALITY_BAD"};