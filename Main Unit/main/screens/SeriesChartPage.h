#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"

#define SERIES_CHART_TITLE_FONT u8g2_font_6x10_tf
#define SERIES_CHART_LABEL_FONT u8g2_font_5x7_tf
#define SERIES_REPORT_PAGES_COUNT (SERIES_SUMMARY_PAGES_COUNT + SERIES_FIELDS_COUNT)


double getSeriesFieldValue(const MeasuredResult &res, SeriesField field)
{
	switch (field)
	{
	case SeriesField::SENSOR0_TIME:
		return res.sensor0Time;
	case SeriesField::SENSOR1_TIME:
		return res.sensor1Time;
	case SeriesField::CURTAIN1_SPAN_A_TIME:
		return res.curtain1spanAtime;
	case SeriesField::CURTAIN1_SPAN_A_SPEED:
		return res.curtain1spanAspeed;
	case SeriesField::CURTAIN1_TOTAL_TIME:
		return res.curtain1TotalTime;
	case SeriesField::CURTAIN2_SPAN_A_TIME:
		return res.curtain2spanAtime;
	case SeriesField::CURTAIN2_SPAN_A_SPEED:
		return res.curtain2spanAspeed;
	case SeriesField::CURTAIN2_TOTAL_TIME:
		return res.curtain2TotalTime;
	case SeriesField::SLIT_WIDTH_SENSOR0:
		return res.slitWidthSensor0;
	case SeriesField::SLIT_WIDTH_SENSOR1:
		return res.slitWidthSensor1;
	default:
		return 0;
	}
}

// Draws text at the cursor position (x, y) with a black plaque behind the actual glyph box.
// The plaque is computed from the text bounding box, so it works with both the classic font
// (y is the top of the glyphs) and u8g2 fonts (y is the baseline).
void drawTextWithPlaque(const char *text, int16_t x, int16_t y)
{
  Rect textBox = getStringRect(text, 0, 0);

  display->fillRect(x + textBox.x - 1, y + textBox.y - 1, textBox.width + 2, textBox.height + 2, BLACK);
  display->setCursor(x, y);
  display->print(text);
}

// Formats shutter speed value (1/s) as a fraction label (e.g. "1/503")
void formatSpeedFractionLabel(double speed, char *buf, size_t bufSize)
{
  char denominator[10];

  if (speed >= 10)
  {
    snprintf(denominator, sizeof(denominator), "%.0f", speed);
  }
  else if (speed >= 1)
  {
    snprintf(denominator, sizeof(denominator), "%.1f", speed);
  }
  else
  {
    snprintf(denominator, sizeof(denominator), "%.2f", speed);
  }

  snprintf(buf, bufSize, "1/%s", denominator);
}

// Formats a chart value according to the field display type
void formatSeriesChartValue(SeriesField field, double chartValue, char *buf, size_t bufSize)
{
  if (seriesFieldDescriptors[(uint8_t)field].displayAsSpeed)
  {
    formatSpeedFractionLabel(chartValue, buf, bufSize);
  }
  else
  {
    formatCompactDouble(chartValue, buf, bufSize);
  }
}

// Returns the value used to build a chart for the given field.
// Sensor time fields are displayed as shutter speed (1/X), other fields as is.
double getSeriesChartValue(const MeasuredResult &res, SeriesField field)
{
  double rawValue = getSeriesFieldValue(res, field);

  if (seriesFieldDescriptors[(uint8_t)field].displayAsSpeed && isSeriesFieldValueValid(rawValue))
  {
    return 1000.0 / rawValue;
  }

  return rawValue;
}

// Draws one chart page: field values for each measurement in the series
void drawSeriesChartPage(const MeasurementSeries &series, uint8_t fieldIndex)
{
  const SeriesFieldDescriptor &desc = seriesFieldDescriptors[fieldIndex];

  display->setFont(SERIES_CHART_TITLE_FONT);
  drawStringHCentered(desc.chartTitle, 15);

  display->setFont(SERIES_CHART_LABEL_FONT);

  double values[MEASUREMENT_SERIES_MAX_SIZE];
  uint8_t validIndexes[MEASUREMENT_SERIES_MAX_SIZE];
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < series.storedCount; i++)
  {
    double rawValue = getSeriesFieldValue(series.results[i], (SeriesField)fieldIndex);

    if (!isSeriesFieldValueValid(rawValue))
    {
      continue;
    }

    values[validCount] = getSeriesChartValue(series.results[i], (SeriesField)fieldIndex);
    validIndexes[validCount] = i;
    validCount++;
  }

  const int16_t chartX0 = 5;
  const int16_t chartX1 = 154;
  const int16_t chartY0 = 30;
  const int16_t chartY1 = 100;

  if (validCount == 0)
  {
    display->setFont(SERIES_CHART_TITLE_FONT);
    drawStringHCentered("No valid data", 60);
    return;
  }

  double vmin = values[0];
  double vmax = values[0];

  for (uint8_t k = 1; k < validCount; k++)
  {
    if (values[k] < vmin)
    {
      vmin = values[k];
    }

    if (values[k] > vmax)
    {
      vmax = values[k];
    }
  }

  if (vmax - vmin < 1e-9)
  {
    double pad = fabs(vmax) * 0.25;

    if (pad < 1e-9)
    {
      pad = 1.0;
    }

    vmin -= pad;
    vmax += pad;
  }
  else
  {
    double pad = (vmax - vmin) * 0.1;
    vmin -= pad;
    vmax += pad;
  }

  // first/last data points (and their ticks) are inset 5 px from the chart edges
  const int16_t pointAreaX0 = chartX0 + 5;
  const int16_t pointAreaX1 = chartX1 - 5;

  // horizontal gridlines
  const uint8_t gridLineCount = 4;

  for (uint8_t g = 0; g < gridLineCount; g++)
  {
    double fraction = (double)g / (double)(gridLineCount - 1);
    int16_t gridY = chartY0 + (int16_t)((chartY1 - chartY0) * fraction);

    display->drawLine(chartX0, gridY, chartX1, gridY, WHITE);
  }

  // axes
  display->drawLine(chartX0, chartY0, chartX0, chartY1, WHITE);
  display->drawLine(chartX0, chartY1, chartX1, chartY1, WHITE);

  // x axis ticks and measurement number labels
  uint8_t totalMeasurements = series.storedCount > 0 ? series.storedCount : 1;

  for (uint8_t i = 0; i < totalMeasurements; i++)
  {
    double fraction = totalMeasurements == 1 ? 0.5 : (double)i / (double)(totalMeasurements - 1);
    int16_t tickX = pointAreaX0 + (int16_t)((pointAreaX1 - pointAreaX0) * fraction);

    display->drawLine(tickX, chartY1, tickX, chartY1 + 3, WHITE);

    char numberLabel[4];
    snprintf(numberLabel, sizeof(numberLabel), "%u", (unsigned)(i + 1));
    Rect numberRect = getStringRect(numberLabel, 0, 0);

    // place the label right below the tick (numberRect.y is negative for u8g2 fonts)
    display->setCursor(tickX - numberRect.width / 2, chartY1 + 4 - numberRect.y);
    display->print(numberLabel);
  }

  // data points
  int16_t pointX[MEASUREMENT_SERIES_MAX_SIZE];
  int16_t pointY[MEASUREMENT_SERIES_MAX_SIZE];

  for (uint8_t k = 0; k < validCount; k++)
  {
    double xFraction = validCount == 1 ? 0.5 : (double)validIndexes[k] / (double)(totalMeasurements - 1);
    pointX[k] = pointAreaX0 + (int16_t)((pointAreaX1 - pointAreaX0) * xFraction);

    double yFraction = (values[k] - vmin) / (vmax - vmin);
    pointY[k] = chartY1 - (int16_t)((chartY1 - chartY0) * yFraction);
  }

  // polyline between adjacent (by measurement number) valid points
  for (uint8_t k = 0; k + 1 < validCount; k++)
  {
    if (validIndexes[k + 1] != validIndexes[k] + 1)
    {
      continue;
    }

    display->drawLine(pointX[k], pointY[k], pointX[k + 1], pointY[k + 1], GREEN);
  }

  for (uint8_t k = 0; k < validCount; k++)
  {
    display->fillCircle(pointX[k], pointY[k], 2, GREEN);
  }

  // find min and max points to label them
  uint8_t minPointIndex = 0;
  uint8_t maxPointIndex = 0;

  for (uint8_t k = 1; k < validCount; k++)
  {
    if (values[k] < values[minPointIndex])
    {
      minPointIndex = k;
    }

    if (values[k] > values[maxPointIndex])
    {
      maxPointIndex = k;
    }
  }

  // label all points for small series, otherwise only min and max
  bool labelAllPoints = validCount <= 5;

  for (uint8_t k = 0; k < validCount; k++)
  {
    if (!labelAllPoints && k != minPointIndex && k != maxPointIndex)
    {
      continue;
    }

    bool drawAbove = labelAllPoints ? (k % 2 == 0) : true;

    // flip the label side if its box does not fit (offsets are tuned for u8g2_font_5x7_tf)
    if (drawAbove && pointY[k] - 10 < chartY0)
    {
      drawAbove = false;
    }

    if (!drawAbove && pointY[k] + 9 > chartY1 + 2)
    {
      drawAbove = true;
    }

    int16_t labelY = drawAbove ? pointY[k] - 4 : pointY[k] + 10;

    char label[14];
    formatSeriesChartValue((SeriesField)fieldIndex, values[k], label, sizeof(label));
    Rect labelRect = getStringRect(label, 0, 0);

    int16_t labelX = pointX[k] - labelRect.width / 2;

    if (labelX < 1)
    {
      labelX = 1;
    }

    if (labelX + labelRect.width > display->width() - 1)
    {
      labelX = display->width() - 1 - labelRect.width;
    }

    drawTextWithPlaque(label, labelX, labelY);
  }

  display->setFont(u8g2_font_6x13_tf);
}