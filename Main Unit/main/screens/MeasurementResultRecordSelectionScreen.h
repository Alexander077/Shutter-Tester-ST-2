#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
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

// Record list shows user names (ai task 6): small font, up to 2 rows per
// record, trailing spaces trimmed. Both record types expose .name, so one
// template helper serves both storages.
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

void splitNameTwoLines(const char *name, char *line1, char *line2, uint8_t charsPerLine)
{
  line1[0] = '\0';
  line2[0] = '\0';

  uint8_t len = strlen(name);

  if (len <= charsPerLine)
  {
    strcpy(line1, name);
    return;
  }

  int split = -1;

  for (int i = charsPerLine; i > 0; i--)
  {
    if (name[i] == ' ' || name[i] == '\0')
    {
      split = i;
      break;
    }
  }

  if (split <= 0)
  {
    split = charsPerLine;
  }

  uint8_t l1len = split;

  while (l1len > 0 && name[l1len - 1] == ' ')
  {
    l1len--;
  }

  strncpy(line1, name, l1len);
  line1[l1len] = '\0';

  uint8_t start = split;

  while (name[start] == ' ')
  {
    start++;
  }

  uint8_t l2len = 0;

  while (name[start + l2len] != '\0' && l2len < charsPerLine)
  {
    l2len++;
  }

  strncpy(line2, &name[start], l2len);
  line2[l2len] = '\0';
}

template <typename StorageT>
int32_t drawRecordSelectionScreen(StorageT &store)
{
  std::vector<int32_t> validIds = store.getAllValidRecordNumbers();

  int16_t totalArraySaize = validIds.size() + 1;
  int32_t recordNumbers[totalArraySaize];

  recordNumbers[0] = 0;

  for (size_t i = 0; i < validIds.size(); i++)
  {
    recordNumbers[i + 1] = validIds[i];
  }

  ace_sorting::shellSortKnuth(recordNumbers, totalArraySaize);

  // Small-font two-row items: default font (~6px advance) -> 23 chars/row
  // at x=20. 40-char names always fit into exactly 2 rows.
  static const uint8_t kX = 20;
  static const uint8_t kArrowX = 8;
  static const int16_t kY = 34;
  static const int16_t kLineH = 9;
  static const int16_t kItemH = 22;
  static const uint8_t kCharsPerRow = 23;
  static const int16_t kPageSize = 3;

  int16_t totalPagesCount = (totalArraySaize + kPageSize - 1) / kPageSize;
  int16_t startEncoderVal = AlexEncoder::counter;
  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);
  drawStringHCentered("Select record", 16);

  auto drawPage = [&](int16_t page)
  {
    display->fillRect(0, kY - 8, display->width(), kPageSize * kItemH, BLACK);
    display->setFont();
    display->setTextColor(WHITE);

    for (int16_t slot = 0; slot < kPageSize; slot++)
    {
      int16_t idx = (page - 1) * kPageSize + slot;

      if (idx >= totalArraySaize)
      {
        break;
      }

      char dispName[RECORD_NAME_BUF_SIZE];
      getRecordDisplayName(store, recordNumbers[idx], dispName);

      char line1[kCharsPerRow + 1];
      char line2[kCharsPerRow + 1];
      splitNameTwoLines(dispName, line1, line2, kCharsPerRow);

      int16_t y0 = kY + slot * kItemH;
      display->setCursor(kX, y0);
      display->print(line1);

      if (line2[0] != '\0')
      {
        display->setCursor(kX, y0 + kLineH);
        display->print(line2);
      }
    }

    display->fillRect(0, 112, display->width(), 16, BLACK);
    display->setFont(u8g2_font_6x13_tf);
    display->setTextColor(WHITE);
    char pageBuf[24];
    snprintf(pageBuf, sizeof(pageBuf), "Page %d of %d", page, totalPagesCount);
    drawStringHCentered(pageBuf, 122);
    display->setFont();
    display->setTextColor(WHITE);
  };

  auto eraseArrowAt = [&](int16_t slot)
  {
    display->setFont();
    display->setTextColor(BLACK);
    display->setCursor(kArrowX, kY + slot * kItemH);
    display->print("->");
    display->setTextColor(WHITE);
  };

  auto drawArrowAt = [&](int16_t slot)
  {
    display->setFont();
    display->setTextColor(WHITE);
    display->setCursor(kArrowX, kY + slot * kItemH);
    display->print("->");
  };

  int16_t prevIndex = 0;
  int16_t prevPage = 1;
  drawPage(1);
  drawArrowAt(0);

  while (true)
  {
    if (isApiRequestReceived)
      return 0;

    int16_t curIndex = AlexEncoder::counter - startEncoderVal;

    if (curIndex > totalArraySaize - 1)
    {
      startEncoderVal = AlexEncoder::counter - (totalArraySaize - 1);
      curIndex = totalArraySaize - 1;
    }

    if (curIndex < 0)
    {
      curIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    if (prevIndex != curIndex)
    {
      int16_t curPage = curIndex / kPageSize + 1;

      if (curPage != prevPage)
      {
        drawPage(curPage);
        drawArrowAt(curIndex % kPageSize);
        prevPage = curPage;
      }
      else
      {
        eraseArrowAt(prevIndex % kPageSize);
        drawArrowAt(curIndex % kPageSize);
      }

      prevIndex = curIndex;
    }

    if (button.isClicked())
    {
      return recordNumbers[curIndex];
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

int32_t drawMeasurementResultRecordSelectionScreen()
{
  return drawRecordSelectionScreen(storage);
}

int32_t drawSeriesRecordSelectionScreen()
{
  return drawRecordSelectionScreen(seriesStorage);
}