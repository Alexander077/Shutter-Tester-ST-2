#pragma once

#include "Derection.h"

struct MeasuredResult
{
  Derection curtainMovementDirection;

  double sensor0Time; // In milliseconds
  double sensor1Time; // In milliseconds

  double curtain1spanAtime; // In milliseconds
  double curtain1spanAspeed;//In meters per second
  // double curtain1FrameAvgSpeed;
  double curtain1TotalTime; // In milliseconds

  double curtain2spanAtime;  // In milliseconds
  double curtain2spanAspeed; // In meters per second
  // double curtain2FrameAvgSpeed;
  double curtain2TotalTime; // In milliseconds

  double slitWidthSensor0;//in mm
  double slitWidthSensor1; // in mm
  double slitWidthAverage; // in mm
};