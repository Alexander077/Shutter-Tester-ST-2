#pragma once

#include "Derection.h"

struct MeasuredResult
{
  Derection curtainMovementDirection;

  double sensor0Time;
  double sensor1Time;
  double sensor2Time;

  double curtain1spanAspeed;
  double curtain1spanBspeed;
  double curtain1spanCspeed;
  double curtain1FrameAvgSpeed;
  double curtain1spanAtime;
  double curtain1spanBtime;
  double curtain1spanCtime;
  double curtain1TotalTime;

  double curtain2spanAspeed;
  double curtain2spanBspeed;
  double curtain2spanCspeed;
  double curtain2FrameAvgSpeed;
  double curtain2spanAtime;
  double curtain2spanBtime;
  double curtain2spanCtime;
  double curtain2TotalTime;

  double slitWidth;
};