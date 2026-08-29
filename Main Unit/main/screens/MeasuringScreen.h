#include "../lib/MeasuredResult.h"
#include "screensCommon.h"

void drawMeasuringScreen()
{
  RawMeasurementCapture capture = captureMeasurement("MEASURING", "Release camera shutter");

  if (capture.isCancelled || isApiRequestReceived)
  {
    return;
  }

  drawMeasuredScreen(capture.sensor0PulseWidthUs,
                     capture.sensor1PulseWidthUs,
                     capture.curtain1TimeUs,
                     capture.curtain2TimeUs);
}