#pragma once

#include <Arduino.h>

class AlexEncoder
{
private:
  static uint8_t _pinA;
  static uint8_t _pinB;
  static int currentStateA;
  static int lastStateA;

  static void updateEncoder();

public:
  static bool currentDir;
  static int counter;
  static void init(uint8_t, uint8_t);
};

uint8_t  AlexEncoder::_pinA = 0;
uint8_t  AlexEncoder::_pinB = 0;
int  AlexEncoder::counter = 0;
int  AlexEncoder::currentStateA = 0;
int  AlexEncoder::lastStateA = 0;
bool  AlexEncoder::currentDir = true;

void AlexEncoder::updateEncoder()
{
  // Read the current state of CLK
  currentStateA = digitalRead(AlexEncoder::_pinA);

  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (AlexEncoder::currentStateA != AlexEncoder::lastStateA && AlexEncoder::currentStateA == 1)
  {

    // If the DT state is different than the CLK state then
    // the encoder is rotating CCW so decrement
    if (digitalRead(AlexEncoder::_pinB) != AlexEncoder::currentStateA)
    {
      AlexEncoder::counter--;
      AlexEncoder::currentDir = false;
    }
    else
    {
      // Encoder is rotating CW so increment
      AlexEncoder::counter++;
      AlexEncoder::currentDir = true;
    }

    Serial.print("Direction: ");
    Serial.print(AlexEncoder::currentDir);
    Serial.print(" | Counter: ");
    Serial.println(AlexEncoder::counter);
  }

  // Remember last CLK state
  AlexEncoder::lastStateA = AlexEncoder::currentStateA;
}


void AlexEncoder::init(uint8_t pinA, uint8_t pinB)
{
  AlexEncoder::_pinA = pinA;
  AlexEncoder::_pinB = pinB;

  pinMode(AlexEncoder::_pinA, INPUT);
  pinMode(AlexEncoder::_pinB, INPUT);

  AlexEncoder::lastStateA = digitalRead(AlexEncoder::_pinA);

  attachInterrupt(0, AlexEncoder::updateEncoder, CHANGE);
  attachInterrupt(1, AlexEncoder::updateEncoder, CHANGE);
}

