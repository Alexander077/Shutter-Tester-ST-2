#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"

#define SERIES_SUMMARY_FIELDS_PER_PAGE 5

// getSeriesFieldValue() and formatCompactDouble() are shared in screensCommon.h

struct SeriesFieldStats
{
  double avg;
  double min;
  double max;
  uint8_t validCount;
};

void computeSeriesFieldStats(const MeasurementSeries &series, SeriesField field, SeriesFieldStats &stats)
{
  double sum = 0;
  stats.avg = 0;
  stats.min = 0;
  stats.max = 0;
  stats.validCount = 0;

  for (uint8_t i = 0; i < series.storedCount; i++)
  {
    double value = getSeriesFieldValue(series.results[i], field);

    if (!isSeriesFieldValueValid(value))
    {
      continue;
    }

    if (stats.validCount == 0)
    {
      stats.min = value;
      stats.max = value;
    }
    else
    {
      if (value < stats.min)
      {
        stats.min = value;
      }

      if (value > stats.max)
      {
        stats.max = value;
      }
    }

    sum += value;
    stats.validCount++;
  }

  if (stats.validCount > 0)
  {
    stats.avg = sum / stats.validCount;
  }
}

// Draws one summary page: up to SERIES_SUMMARY_FIELDS_PER_PAGE fields with
// avg/min/max values. Only fields that have valid data in the series are shown
// (activeFields), so there are no pages full of N/A for e.g. leaf shutters.
void drawSeriesSummaryPage(const MeasurementSeries &series, const SeriesField *activeFields, uint8_t activeFieldsCount, uint8_t pageIndex, uint8_t summaryPagesCount)
{
  char title[16];
  snprintf(title, sizeof(title), "Summary %u/%u", (unsigned)(pageIndex + 1), (unsigned)summaryPagesCount);
  drawStringHCentered(title, 15);

  display->setFont(); // small font for the table

  // Value columns are right-aligned: long values grow to the left and never
  // cross the right screen edge
  const int16_t labelX = 2;
  const int16_t avgRightX = 94;
  const int16_t minRightX = 126;
  const int16_t maxRightX = 158;

  display->setCursor(labelX, 28);
  display->print("Parameter");
  drawStringRightAligned("Avg", avgRightX, 28);
  drawStringRightAligned("Min", minRightX, 28);
  drawStringRightAligned("Max", maxRightX, 28);

  char buf[14];

  for (uint8_t row = 0; row < SERIES_SUMMARY_FIELDS_PER_PAGE; row++)
  {
    uint8_t fieldSlot = pageIndex * SERIES_SUMMARY_FIELDS_PER_PAGE + row;

    if (fieldSlot >= activeFieldsCount)
    {
      break;
    }

    SeriesField field = activeFields[fieldSlot];
    const SeriesFieldDescriptor &desc = seriesFieldDescriptors[(uint8_t)field];
    int16_t rowY = 40 + row * 15;

    display->setCursor(labelX, rowY);
    display->print(desc.tableLabel);

    SeriesFieldStats stats;
    computeSeriesFieldStats(series, field, stats);

    if (stats.validCount == 0)
    {
      drawStringRightAligned("N/A", avgRightX, rowY);
      drawStringRightAligned("N/A", minRightX, rowY);
      drawStringRightAligned("N/A", maxRightX, rowY);
      continue;
    }

    formatCompactDouble(stats.avg, buf, sizeof(buf));
    drawStringRightAligned(buf, avgRightX, rowY);

    formatCompactDouble(stats.min, buf, sizeof(buf));
    drawStringRightAligned(buf, minRightX, rowY);

    formatCompactDouble(stats.max, buf, sizeof(buf));
    drawStringRightAligned(buf, maxRightX, rowY);
  }

  display->setFont(u8g2_font_6x13_tf);
}