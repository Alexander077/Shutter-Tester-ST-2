#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "lib/MeasurementSeries.h"
#include "screens/SeriesSummaryPage.h"
#include "screens/SeriesChartPage.h"
#include "lib/serialAPI.h"

// Multi-page series report: 2 summary pages + 1 chart page per field
void renderSeriesReport(MeasurementSeries &series)
{
  const int16_t totalPagesCount = SERIES_REPORT_PAGES_COUNT;
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

      if (resultPageIndex < SERIES_SUMMARY_PAGES_COUNT)
      {
        drawSeriesSummaryPage(series, (uint8_t)resultPageIndex);
      }
      else
      {
        drawSeriesChartPage(series, (uint8_t)(resultPageIndex - SERIES_SUMMARY_PAGES_COUNT));
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