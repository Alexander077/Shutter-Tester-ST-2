#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "recordsManagement/RecordsManagementCommon.h"
#include "lib/AceSorting/src/AceSorting.h"
#include "lib/serialAPI.h"
#include <mString.h>
#include <string.h>
#include <stdio.h>

inline int32_t drawMeasurementResultRecordSelectionScreen()
{
  // 1. Получаем все существующие (не удаленные) номера записей через менеджер
  std::vector<int32_t> validIds = storage.getAllValidRecordNumbers();

  // 2. Размер массива = количество записей + 1 (для кнопки "Back")
  int16_t totalArraySaize = validIds.size() + 1;
  int32_t recordNumbers[totalArraySaize];

  recordNumbers[0] = 0; // Опция "Back" (возврат)

  // 3. Заполняем массив данными из вектора
  for (size_t i = 0; i < validIds.size(); i++)
  {
    recordNumbers[i + 1] = validIds[i];
  }

  // Сортировка (оставляем вашу оригинальную логику)
  ace_sorting::shellSortKnuth(recordNumbers, totalArraySaize);

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
  const int16_t recTitleBufSize = 30;
  int16_t totalPagesCount = ceil((double)totalArraySaize / (double)pageSize);
  drawStringHCentered("Select record", 20);
  int8_t itemsCountToPrint = min(pageSize, totalArraySaize);

  for (uint8_t i = 0; i < itemsCountToPrint; i++)
  {
    display->setCursor(xMargin, yMargin + (ySpacing * i));

    // int32_t recordNumber = getInputParamsArrayInt(i + 3);
    int32_t recordNumber = recordNumbers[i];

    if (recordNumber == 0) //"Back" option
    {
      display->print("Back");
      continue;
    }

    char recTitle[recTitleBufSize];
    formatRecordName(recordNumber, recTitle);
    display->print(recTitle);

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
          // int32_t recordNumber = getInputParamsArrayInt(i + 3);
          int32_t recordNumber = recordNumbers[i];
          char recTitle[recTitleBufSize];
          formatRecordName(recordNumber, recTitle);

          if (recordNumber == 0) //"Back" option
          {
            display->print("Back");
            continue;
          }

          display->print(recTitle);
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
          // int32_t recordNumber = getInputParamsArrayInt(i + 3);
          int32_t recordNumber = recordNumbers[i];

          if (recordNumber == 0) //"Back" option
          {
            display->print("Back");
            continue;
          }

          char recTitle[recTitleBufSize];
          formatRecordName(recordNumber, recTitle);
          // ESP_LOGI("", "%" PRIi32, recordNumber);
          display->print(recTitle);
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
      return recordNumbers[curIndex];
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}