#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/CaptureMeasurement.h"
#include "screens/computeMeasuredResult.h"
#include "screens/MessageScreen.h"
#include "screens/SeriesReport.h"
#include "screens/SeriesSaveScreen.h"
#include "lib/serialAPI.h"

MeasurementSeries measurementSeries = {};

// Called when a series ends prematurely (cancelled by the button or aborted
// after a failed measurement) while some results are already stored.
// Lets the user view the partial report instead of losing all measurements.
void offerPartialSeriesReport(const char *abortTitle)
{
  if (measurementSeries.storedCount == 0)
  {
    return;
  }

  const char *abortOptions[] = {"Show partial report", "Discard series"};
  int16_t abortOptionIndex = drawMessageScreen(abortTitle, 2, abortOptions);

  if (isApiRequestReceived)
  {
    return;
  }

  if (abortOptionIndex == 0) // Show partial report
  {
    renderSeriesReport(measurementSeries);

    if (isApiRequestReceived)
      return;

    drawSeriesSaveScreen(measurementSeries);
  }
}

// Runs a series of measurements one by one and shows the multi-page report at the end
void drawMeasuringSeriesScreen(uint8_t seriesCount)
{
  // Defensive clamp: results[] holds at most MEASUREMENT_SERIES_MAX_SIZE entries
  if (seriesCount > MEASUREMENT_SERIES_MAX_SIZE)
  {
    seriesCount = MEASUREMENT_SERIES_MAX_SIZE;
  }

  measurementSeries.totalCount = seriesCount;
  measurementSeries.storedCount = 0;

  for (uint8_t measurementIndex = 0; measurementIndex < seriesCount; measurementIndex++)
  {
    char measuringTitle[24];
    snprintf(measuringTitle, sizeof(measuringTitle), "Measurement %u of %u", (unsigned)(measurementIndex + 1), (unsigned)seriesCount);

    bool repeatCurrentMeasurement = true;

    while (repeatCurrentMeasurement)
    {
      repeatCurrentMeasurement = false;

      RawMeasurementCapture capture = captureMeasurement("MEASURING", measuringTitle, "Release camera shutter");

      if (isApiRequestReceived)
      {
        return;
      }

      if (capture.isCancelled)
      {
        offerPartialSeriesReport("Series cancelled");
        return;
      }

      ComputedMeasurement computed;
      computeMeasuredResult(capture.sensor0PulseWidthUs,
                            capture.sensor1PulseWidthUs,
                            capture.curtain1TimeUs,
                            capture.curtain2TimeUs,
                            computed);

      // Captures with dim/bright light, too short or otherwise invalid data
      // get negative sentinel values in sensor times. A series always requires
      // both sensors to be valid so the statistics never mix partial data.
      bool isCaptureValid = computed.res.sensor0Time > 0 && computed.res.sensor1Time > 0;

      if (!isCaptureValid)
      {
        const char *failOptions[] = {"Retry", "Abort series"};
        int16_t failOptionIndex = drawMessageScreen("Measurement failed", 2, failOptions);

        if (isApiRequestReceived)
        {
          return;
        }

        if (failOptionIndex == 0) // Retry
        {
          repeatCurrentMeasurement = true;
          continue;
        }

        offerPartialSeriesReport("Series aborted"); // Abort series
        return;
      }

      measurementSeries.results[measurementIndex] = computed.res;
      measurementSeries.storedCount++;
    }
  }

  renderSeriesReport(measurementSeries);

  if (isApiRequestReceived)
    return;

  drawSeriesSaveScreen(measurementSeries);
}