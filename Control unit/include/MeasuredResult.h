#pragma once

#include "Derection.h"

struct MeasuredResult
{
  Derection curtainMovementDirection;

  double sensor0Time;
  double sensor1Time;

  double curtain1spanAtime;
  double curtain1spanAspeed;
  double curtain1FrameAvgSpeed;
  double curtain1TotalTime;

  double curtain2spanAtime;
  double curtain2spanAspeed;
  double curtain2FrameAvgSpeed;
  double curtain2TotalTime;

  double slitWidthSpanA;
};