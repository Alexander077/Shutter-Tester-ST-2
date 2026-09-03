#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/SeriesSummaryPage.h"
#include "screens/SeriesChartPage.h"
#include "lib/serialAPI.h"

// Multi-page series report: summary pages (up to 5 fields per page) + 1 chart
// page per field. Fields with no valid data in the whole series (e.g. curtain
// and slit data for leaf shutters) are skipped entirely.
void renderSeriesReport(MeasurementSeries &series)
{
  SeriesField activeFields[SERIES_FIELDS_COUNT];
  uint8_t activeFieldsCount = buildActiveSeriesFields(series, activeFields);

  uint8_t summaryPagesCount =
      (activeFieldsCount + SERIES_SUMMARY_FIELDS_PER_PAGE - 1) / SERIES_SUMMARY_FIELDS_PER_PAGE;

  if (summaryPagesCount == 0)
  {
    summaryPagesCount = 1; // always show at least one (possibly empty) summary page
  }

  const int16_t totalPagesCount = summaryPagesCount + activeFieldsCount;
  int16_t startEncoderVal = AlexEncoder::counter;
  int8_t prevPageIndex = -1;
  display->fillScreen(BLACK);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextColor(WHITE);

  while (true)
  {
    if (isApiRequestReceived)
      return;

    int16_t resultPageIndex = AlexEncoder::counter - startEncoderVal;

    if (resultPageIndex > totalPagesCount - 1)
    {
      startEncoderVal = AlexEncoder::counter - (totalPagesCount - 1);
      resultPageIndex = totalPagesCount - 1;
    }

    if (resultPageIndex < 0)
    {
      resultPageIndex = 0;
      startEncoderVal = AlexEncoder::counter;
    }

    if (resultPageIndex != prevPageIndex)
    {
      display->fillScreen(BLACK);

      if (resultPageIndex < summaryPagesCount)
      {
        drawSeriesSummaryPage(series, activeFields, activeFieldsCount, (uint8_t)resultPageIndex, summaryPagesCount);
      }
      else
      {
        drawSeriesChartPage(series, (uint8_t)activeFields[resultPageIndex - summaryPagesCount]);
      }

      drawNavBar(resultPageIndex, totalPagesCount);
      prevPageIndex = resultPageIndex;
    }

    if (button.isClicked())
    {
      return;
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}