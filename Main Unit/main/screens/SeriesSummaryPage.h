#include "../lib/MeasuredResult.h"
#include "screensCommon.h"

// Draws one summary page: 5 fields with avg/min/max values
void drawSeriesSummaryPage(const MeasurementSeries &series, uint8_t pageIndex)
{
  char title[16];
  snprintf(title, sizeof(title), "Summary %u/%u", (unsigned)(pageIndex + 1), (unsigned)SERIES_SUMMARY_PAGES_COUNT);
  drawStringHCentered(title, 15);

  display->setFont(); // small font for the table

  const int16_t labelX = 2;
  const int16_t avgX = 62;
  const int16_t minX = 94;
  const int16_t maxX = 126;

  display->setCursor(labelX, 28);
  display->print("Parameter");
  display->setCursor(avgX, 28);
  display->print("Avg");
  display->setCursor(minX, 28);
  display->print("Min");
  display->setCursor(maxX, 28);
  display->print("Max");

  char buf[14];

  for (uint8_t row = 0; row < SERIES_SUMMARY_FIELDS_PER_PAGE; row++)
  {
    uint8_t fieldIndex = pageIndex * SERIES_SUMMARY_FIELDS_PER_PAGE + row;

    if (fieldIndex >= SERIES_FIELDS_COUNT)
    {
      break;
    }

    const SeriesFieldDescriptor &desc = seriesFieldDescriptors[fieldIndex];
    int16_t rowY = 40 + row * 15;

    display->setCursor(labelX, rowY);
    display->print(desc.tableLabel);

    SeriesFieldStats stats;
    computeSeriesFieldStats(series, (SeriesField)fieldIndex, stats);

    if (stats.validCount == 0)
    {
      display->setCursor(avgX, rowY);
      display->print("N/A");

      display->setCursor(minX, rowY);
      display->print("N/A");

      display->setCursor(maxX, rowY);
      display->print("N/A");
      continue;
    }

    display->setCursor(avgX, rowY);
    formatCompactDouble(stats.avg, buf, sizeof(buf));
    display->print(buf);

    display->setCursor(minX, rowY);
    formatCompactDouble(stats.min, buf, sizeof(buf));
    display->print(buf);

    display->setCursor(maxX, rowY);
    formatCompactDouble(stats.max, buf, sizeof(buf));
    display->print(buf);
  }

  display->setFont(u8g2_font_6x13_tf);
}