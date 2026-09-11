#pragma once

#include <string.h>
#include "lib/MeasuredResult.h"

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

  // Пользовательское имя серии (до RECORD_NAME_MAX_LEN символов,
  // пустая строка = имя не задано, показывается номер записи)
  char name[RECORD_NAME_BUF_SIZE];
};
