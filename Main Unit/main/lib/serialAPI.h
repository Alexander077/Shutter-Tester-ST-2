
class SerialAPILightStatus
{
public:
  static constexpr const char *LIGHT_STATUS_TOO_DIMM = "LIGHT_STATUS_TOO_DIM";
  static constexpr const char *LIGHT_STATUS_TOO_BRIGHT = "LIGHT_STATUS_TOO_BRIGHT";
  static constexpr const char *LIGHT_STATUS_OK = "LIGHT_STATUS_OK";
};

char *serialApiLightQualityStatusesStr[3] = {"LIGHT_QUALITY_UNKNOWN", "LIGHT_QUALITY_OK", "LIGHT_QUALITY_BAD"};