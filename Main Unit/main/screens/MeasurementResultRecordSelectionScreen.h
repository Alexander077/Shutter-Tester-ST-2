#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "lib/AceSorting/src/AceSorting.h"
#include "lib/serialAPI.h"
#include <mString.h>
#include <string.h>
#include <stdio.h>

// Элемент списка выбора записи: номер + отображаемый заголовок
// (имя записи, либо форматированный номер, если имя не задано)
template <typename RecordT>
struct RecordSelectionItem
{
  int32_t recordNumber;
  char title[30];
};

// Формирует заголовок списка: имя записи (обрезанное под ширину строки),
// либо форматированный номер, если имя пустое
template <typename RecordT>
void formatRecordSelectionTitle(const RecordT &rec, char *out, int16_t bufSize)
{
  const int16_t maxVisibleChars = 16; // (128 - xMargin 30) / 6 px на символ

  int16_t len = 0;
  while (rec.name[len] != '\0' && len < RECORD_NAME_MAX_LEN)
  {
    len++;
  }

  if (len == 0)
  {
    formatRecordName(rec.recordNumber, out);
    return;
  }

  if (len > maxVisibleChars)
  {
    int16_t keep = maxVisibleChars - 3;

    for (int16_t i = 0; i < keep; i++)
    {
      out[i] = rec.name[i];
    }

    out[keep] = '.';
    out[keep + 1] = '.';
    out[keep + 2] = '.';
    out[keep + 3] = '\0';
  }
  else
  {
    snprintf(out, bufSize, "%s", rec.name);
  }
}

template <typename StorageT, typename RecordT>
int32_t drawRecordSelectionScreen(StorageT &store)
{
  // 1. Получаем все существующие (не удаленные) записи через менеджер
  std::vector<RecordT> validRecords = store.getAllValidRecords();

  // 2. Размер массива = количество записей + 1 (для кнопки "Back")
  int16_t totalArraySaize = validRecords.size() + 1;
  RecordSelectionItem<RecordT> items[totalArraySaize];

  items[0].recordNumber = 0; // Опция "Back" (возврат)
  snprintf(items[0].title, sizeof(items[0].title), "Back");

  // 3. Заполняем список (имя записи либо номер, если имя не задано)
  for (size_t i = 0; i < validRecords.size(); i++)
  {
    items[i + 1].recordNumber = validRecords[i].recordNumber;
    formatRecordSelectionTitle(validRecords[i], items[i + 1].title, sizeof(items[i + 1].title));
  }

  // Сортировка по номеру записи
  ace_sorting::shellSortKnuth(items, (uint16_t)totalArraySaize,
      [](const RecordSelectionItem<RecordT> &a, const RecordSelectionItem<RecordT> &b)
      {
        return a.recordNumber < b.recordNumber;
      });

  int16_t startEncoderVal = AlexEncoder::counter;
  display->fillScreen(BLACK);

  uint8_t xMargin = 30;
  uint8_t yMargin = 40;
  uint8_t ySpacing = 15;
  uint8_t arrowXmargin = 15;
  uint8_t curPageLabelY = 120;
  mString<30> pageLabel;
  const int16_t pageSize = 5;
  int16_t prevPage = 1;
  int16_t curPage = 1;
  int16_t totalPagesCount = ceil((double)totalArraySaize / (double)pageSize);
  drawStringHCentered("Select record", 20);
  int8_t itemsCountToPrint = min(pageSize, totalArraySaize);

  for (uint8_t i = 0; i < itemsCountToPrint; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));

    if (items[i].recordNumber == 0) //"Back" option
    {
      display->print("Back");
      continue;
    }

    display->print(items[i].title);

    pageLabel.clear();
    pageLabel += "Page ";
    pageLabel.add(curPage);
    pageLabel += " of ";
    pageLabel.add(totalPagesCount);
    drawStringHCentered(pageLabel, curPageLabelY);
  }

  display->setCursor(xMargin - arrowXmargin, yMargin);
  display->print("->");

  int8_t prevSelectedRecordIndex = 0;
  int8_t topRecordIndex = 0;
  int8_t prevTopRecordIndex = 0;

  while (true)
  {
    if (isApiRequestReceived)
      return 0; // ВЫХОД ДЛЯ API

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

    // ----------------------------
    // displayManager.drawMeasurementResultRecordSelectionScreen(curIndex, recordNumbers, totalArraySaize);

    if (prevSelectedRecordIndex != curIndex)
    {
      display->setTextColor(BLACK);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * (prevSelectedRecordIndex % pageSize)));
      display->print("->");

      display->setTextColor(WHITE);
      display->setCursor(xMargin - arrowXmargin, yMargin + (ySpacing * (curIndex % pageSize)));
      display->print("->");

      prevSelectedRecordIndex = curIndex;

      curPage = ceil((curIndex + 1) / (double)pageSize);
      // ESP_LOGI("", "Cur. page: %" PRIi16, curPage);

      if (prevPage != curPage)
      {
        // ------------------ erase cur page
        display->setTextColor(BLACK);

        int16_t prevPageMaxIndex = pageSize * (prevPage - 1) + pageSize;
        int16_t prevPageLimit = min(prevPageMaxIndex, totalArraySaize);

        for (uint8_t i = pageSize * (prevPage - 1), rowCounter = 0; i < prevPageLimit; i++, rowCounter++)
        {
          display->setCursor(xMargin, yMargin + (ySpacing * rowCounter));

          if (items[i].recordNumber == 0) //"Back" option
          {
            display->print("Back");
            continue;
          }

          display->print(items[i].title);
        }

        pageLabel.clear();
        pageLabel += "Page ";
        pageLabel.add(prevPage);
        pageLabel += " of ";
        pageLabel.add(totalPagesCount);
        drawStringHCentered(pageLabel, curPageLabelY);
        //--------------------------- end of erase cur page

        display->setTextColor(WHITE);
        int16_t nextPageMaxIndex = pageSize * (curPage - 1) + pageSize;
        int16_t limit = min(nextPageMaxIndex, totalArraySaize);

        // draw new page records
        for (int16_t i = pageSize * (curPage - 1), rowCounter = 0; i < limit; i++, rowCounter++)
        {
          display->setCursor(xMargin, yMargin + (ySpacing * rowCounter));

          if (items[i].recordNumber == 0) //"Back" option
          {
            display->print("Back");
            continue;
          }

          display->print(items[i].title);
        }

        pageLabel.clear();
        pageLabel += "Page ";
        pageLabel.add(curPage);
        pageLabel += " of ";
        pageLabel.add(totalPagesCount);
        drawStringHCentered(pageLabel, curPageLabelY);

        prevPage = curPage;
      }
    }

    // ----------------------------

    if (button.isClicked())
    {
      return items[curIndex].recordNumber;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

int32_t drawMeasurementResultRecordSelectionScreen()
{
  return drawRecordSelectionScreen<RecordsStorageManager, StoredMeasuredResult>(storage);
}

int32_t drawSeriesRecordSelectionScreen()
{
  return drawRecordSelectionScreen<SeriesStorageManager, StoredMeasuredSeries>(seriesStorage);
}