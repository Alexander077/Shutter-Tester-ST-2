#pragma once

#include <string.h>
#include "lib/MeasuredResult.h"
#include "lib/Common.h"

// A single saved measurement series. Stored one-per-slot in seriesRecords.bin (see
// RecordsManagementCommon.h) using the same fixed-size record scheme as
// StoredMeasuredResult. 'measurementCount' holds the number of valid entries
// in results[] (the series report only relies on this count).
struct StoredMeasuredSeries
{
  int32_t recordNumber;
  bool isDeleted;
  uint8_t measurementCount;
  MeasuredResult results[MEASUREMENT_SERIES_MAX_SIZE];

  // User-editable record name, always '\0'-terminated. Appended at the end
  // so the layout of all previous fields is unchanged.
  char name[RECORD_NAME_MAX_LEN + 1];
};
