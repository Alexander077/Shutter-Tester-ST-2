#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "lib/serialAPI.h"
#include <mString.h>
#include <string.h>
#include <stdio.h>

// Two rows per record in the selection list: long names (up to 40 chars) are
// wrapped into two display rows, "Back" stays single-row.
#define RECORD_SELECTION_PAGE_SIZE 2
#define RECORD_SELECTION_ROW_H 13
// Max chars per wrapped row (6 px font on 160 px wide display with margins).
#define RECORD_SELECTION_ROW_CHARS 18

// Splits a record name into up to two display rows (no word wrap, hard cut).
static void splitRecordNameRows(const char *name, char *row0, size_t row0Size, char *row1, size_t row1Size)
{
  row0[0] = '\0';
  row1[0] = '\0';

  if (name == NULL)
  {
    return;
  }

  size_t len = strlen(name);
  size_t n0 = len < (size_t)(RECORD_SELECTION_ROW_CHARS) ? len : (size_t)(RECORD_SELECTION_ROW_CHARS);
  strncpy(row0, name, n0);
  row0[n0] = '\0';

  if (len > (size_t)(RECORD_SELECTION_ROW_CHARS))
  {
    size_t rest = len - (size_t)(RECORD_SELECTION_ROW_CHARS);
    size_t n1 = rest < (size_t)(RECORD_SELECTION_ROW_CHARS) ? rest : (size_t)(RECORD_SELECTION_ROW_CHARS);
    strncpy(row1, name + RECORD_SELECTION_ROW_CHARS, n1);
    row1[n1] = '\0';
  }
  (void)row0Size;
  (void)row1Size;
}

template <typename StorageT, typename RecordT>
static void drawSelectionListItem(uint8_t xMargin, uint8_t yMargin, const RecordT *rec)
{
  if (rec == NULL)
  {
    display->setCursor(xMargin, yMargin);
    display->print("Back");
    return;
  }

  const char *name = rec->name;
  char row0[RECORD_SELECTION_ROW_CHARS + 1];
  char row1[RECORD_SELECTION_ROW_CHARS + 1];
  splitRecordNameRows(name[0] != '\0' ? name : "???", row0, sizeof(row0), row1, sizeof(row1));

  display->setCursor(xMargin, yMargin);
  display->print(row0);
  if (row1[0] != '\0')
  {
    display->setCursor(xMargin, (int16_t)(yMargin + RECORD_SELECTION_ROW_H));
    display->print(row1);
  }
  (void)sizeof(StorageT);
}

template <typename StorageT>
int32_t drawRecordSelectionScreen(StorageT &store)
{
  // Load full valid records (numbers + names) and sort by record number.
  // Index 0 is the virtual "Back" entry.
  typedef typename std::remove_reference<decltype(store.getRecordByNumber(0))>::type RecordType;
  std::vector<RecordType> validRecords = store.getAllValidRecords();

  int16_t totalArraySaize = (int16_t)validRecords.size() + 1;

  // Sort valid records by recordNumber (ascending), keep Back at index 0.
  for (size_t i = 0; i < validRecords.size(); i++)
  {
    for (size_t j = i + 1; j < validRecords.size(); j++)
    {
      if (validRecords[j].recordNumber < validRecords[i].recordNumber)
      {
        RecordType tmp = validRecords[i];
        validRecords[i] = validRecords[j];
        validRecords[j] = tmp;
      }
    }
  }

  int16_t startEncoderVal = AlexEncoder::counter;
  display->fillScreen(BLACK);

  uint8_t xMargin = 30;
  uint8_t yMargin = 40;
  // One list slot = two text rows (name may wrap); slot pitch:
  uint8_t ySpacing = RECORD_SELECTION_ROW_H * 2 + 4;
  uint8_t arrowXmargin = 15;
  uint8_t curPageLabelY = 150;
  mString<30> pageLabel;
  const int16_t pageSize = RECORD_SELECTION_PAGE_SIZE;
  int16_t prevPage = 1;
  int16_t curPage = 1;
  int16_t totalPagesCount = (int16_t)ceil((double)totalArraySaize / (double)pageSize);
  drawStringHCentered("Select record", 20);

  // Prints one page (page1-based). Each slot prints 1 ("Back") or 2 rows.
  auto drawPage = [&](int16_t page)
  {
    int16_t first = (page - 1) * pageSize;
    int16_t last = first + pageSize;
    if (last > totalArraySaize)
    {
      last = totalArraySaize;
    }
    for (int16_t i = first, row = 0; i < last; i++, row++)
    {
      uint8_t slotY = (uint8_t)(yMargin + row * ySpacing);
      if (i == 0)
      {
        drawSelectionListItem<StorageT, RecordType>(xMargin, slotY, NULL);
      }
      else
      {
        drawSelectionListItem<StorageT, RecordType>(xMargin, slotY, &validRecords[(size_t)(i - 1)]);
      }
    }
    pageLabel.clear();
    pageLabel += "Page ";
    pageLabel.add(page);
    pageLabel += " of ";
    pageLabel.add(totalPagesCount);
    display->setTextColor(WHITE);
    drawStringHCentered(pageLabel, curPageLabelY);
  };

  auto erasePage = [&](int16_t page)
  {
    display->setTextColor(BLACK);
    int16_t first = (page - 1) * pageSize;
    int16_t last = first + pageSize;
    if (last > totalArraySaize)
    {
      last = totalArraySaize;
    }
    for (int16_t i = first, row = 0; i < last; i++, row++)
    {
      uint8_t slotY = (uint8_t)(yMargin + row * ySpacing);
      if (i == 0)
      {
        display->setCursor(xMargin, slotY);
        display->print("Back");
      }
      else
      {
        const RecordType &rec = validRecords[(size_t)(i - 1)];
        char row0[RECORD_SELECTION_ROW_CHARS + 1];
        char row1[RECORD_SELECTION_ROW_CHARS + 1];
        const char *nm = rec.name[0] != '\0' ? rec.name : "???";
        splitRecordNameRows(nm, row0, sizeof(row0), row1, sizeof(row1));
        display->setCursor(xMargin, slotY);
        display->print(row0);
        if (row1[0] != '\0')
        {
          display->setCursor(xMargin, (uint8_t)(slotY + RECORD_SELECTION_ROW_H));
          display->print(row1);
        }
      }
    }
    pageLabel.clear();
    pageLabel += "Page ";
    pageLabel.add(page);
    pageLabel += " of ";
    pageLabel.add(totalPagesCount);
    drawStringHCentered(pageLabel, curPageLabelY);
    display->setTextColor(WHITE);
  };

  drawPage(1);

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int16_t prevSelectedRecordIndex = 0;

  // Y of the arrow for a list index (slot top row).
  auto arrowY = [&](int16_t listIndex) -> uint8_t
  {
    return (uint8_t)(yMargin + (listIndex % pageSize) * ySpacing);
  };

  while (true)
  {
    if (isApiRequestReceived)
    {
      return 0; // ВЫХОД ДЛЯ API
    }

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

    if (prevSelectedRecordIndex != curIndex)
    {
      display->setTextColor(BLACK);
      display->setCursor(xMargin - arrowXmargin, arrowY(prevSelectedRecordIndex));
      display->print("->");

      display->setTextColor(WHITE);
      display->setCursor(xMargin - arrowXmargin, arrowY(curIndex));
      display->print("->");

      prevSelectedRecordIndex = curIndex;

      curPage = (int16_t)ceil((curIndex + 1) / (double)pageSize);

      if (prevPage != curPage)
      {
        erasePage(prevPage);
        drawPage(curPage);
        // Re-draw the arrow on the new page (erasePage/drawPage don't touch it).
        display->setTextColor(WHITE);
        display->setCursor(xMargin - arrowXmargin, arrowY(curIndex));
        display->print("->");
        prevPage = curPage;
      }
    }

    if (button.isClicked())
    {
      if (curIndex == 0)
      {
        return 0;
      }
      return validRecords[(size_t)(curIndex - 1)].recordNumber;
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