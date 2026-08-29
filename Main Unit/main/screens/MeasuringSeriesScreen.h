#pragma once

#include "lib/MeasuredResult.h"
#include "screensCommon.h"
#include "screens/CaptureMeasurement.h"
#include "screens/computeMeasuredResult.h"
#include "screens/MessageScreen.h"
#include "screens/SeriesReport.h"
#include "lib/MeasurementSeries.h"
#include "lib/serialAPI.h"

inline MeasurementSeries measurementSeries = {};

// Runs a series of measurements one by one and shows the multi-page report at the end
inline void drawMeasuringSeriesScreen(uint8_t seriesCount)
{
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

      if (capture.isCancelled || isApiRequestReceived)
      {
        return;
      }

      ComputedMeasurement computed;
      computeMeasuredResult(capture.sensor0PulseWidthUs,
                            capture.sensor1PulseWidthUs,
                            capture.curtain1TimeUs,
                            capture.curtain2TimeUs,
                            computed);

      // Captures with dim/bright light, too short or otherwise invalid data
      // get negative sentinel values in sensor times
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

        return; // Abort series
      }

      measurementSeries.results[measurementIndex] = computed.res;
      measurementSeries.storedCount++;
    }
  }

  renderSeriesReport(measurementSeries);
}