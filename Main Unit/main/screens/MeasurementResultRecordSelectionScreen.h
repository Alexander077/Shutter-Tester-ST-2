#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/SelectItemScreen.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "lib/AceSorting/src/AceSorting.h"
#include "lib/serialAPI.h"
#include <mString.h>
#include <string.h>
#include <stdio.h>

// Exclude-id plumbing shared by the save screens and the rename flow in
// ViewRecordsScreen.h (plain function pointers cannot capture, so the flows
// set these before opening the editor).
inline int32_t &measurementSaveExcludeRecordNumberRef()
{
  static int32_t v = -1;
  return v;
}

inline int32_t &seriesSaveExcludeRecordNumberRef()
{
  static int32_t v = -1;
  return v;
}

#define measurementSaveExcludeRecordNumber measurementSaveExcludeRecordNumberRef()
#define seriesSaveExcludeRecordNumber seriesSaveExcludeRecordNumberRef()

inline bool isMeasurementSaveNameDuplicate(const char *name)
{
  return isMeasurementRecordNameTaken(name, measurementSaveExcludeRecordNumber);
}

inline bool isSeriesSaveNameDuplicate(const char *name)
{
  return isSeriesRecordNameTaken(name, seriesSaveExcludeRecordNumber);
}

// Display name of one record for the list screen: user name with trailing
// spaces trimmed (drawSelectItemScreen() renders it with the small font, one
// or two rows). Both record types expose .name, so one template helper serves
// both storages.
template <typename StorageT>
void getRecordDisplayName(StorageT &store, int32_t recordNumber, char *out)
{
  if (recordNumber == 0)
  {
    strcpy(out, "Back");
    return;
  }

  auto rec = store.getRecordByNumber(recordNumber);

  if (rec.recordNumber == -1 || isRecordNameEmpty(rec.name))
  {
    // Fallback to the legacy number style (defensive: the startup backfill
    // already assigns names to API-written records).
    formatDefaultRecordName(recordNumber, out);
    return;
  }

  copyRecordName(out, rec.name);

  uint8_t visLen = getRecordNameVisibleLen(out);
  out[visLen] = '\0';
}

// Paged record list (drawSelectItemScreen with the Go Back item on top).
// Both record types expose .name, so one template helper serves both storages.
// Returns the selected record number, or 0 on Go Back / API abort (the
// original contract kept for the callers).
template <typename StorageT>
int32_t drawRecordSelectionScreen(StorageT &store)
{
  // The names are shown by drawSelectItemScreen(), which keeps pointers to
  // them during the whole interaction - hence the static buffers.
  static const int16_t kMaxRecordsCount = TOTAL_RECORDS_COUNT > TOTAL_SERIES_COUNT ? TOTAL_RECORDS_COUNT : TOTAL_SERIES_COUNT;
  static int32_t recordNumbers[kMaxRecordsCount];
  static char displayNameBuffers[kMaxRecordsCount][RECORD_NAME_BUF_SIZE];
  static const char *displayNames[kMaxRecordsCount];

  std::vector<int32_t> validIds = store.getAllValidRecordNumbers();

  int16_t recordsCount = (int16_t)validIds.size();

  if (recordsCount > kMaxRecordsCount)
  {
    recordsCount = kMaxRecordsCount;
  }

  for (int16_t i = 0; i < recordsCount; i++)
  {
    recordNumbers[i] = validIds[i];
  }

  ace_sorting::shellSortKnuth(recordNumbers, recordsCount);

  for (int16_t i = 0; i < recordsCount; i++)
  {
    getRecordDisplayName(store, recordNumbers[i], displayNameBuffers[i]);
    displayNames[i] = displayNameBuffers[i];
  }

  int16_t selectedIndex = drawSelectItemScreen("Select record", displayNames, recordsCount);

  if (selectedIndex < 0)
  {
    return 0; // Go Back / API abort
  }

  return recordNumbers[selectedIndex];
}

int32_t drawMeasurementResultRecordSelectionScreen()
{
  return drawRecordSelectionScreen(storage);
}

int32_t drawSeriesRecordSelectionScreen()
{
  return drawRecordSelectionScreen(seriesStorage);
}