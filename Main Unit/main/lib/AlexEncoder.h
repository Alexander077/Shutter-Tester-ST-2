#pragma once

#include "Arduino.h"

class AlexEncoder
{
private:
  static volatile uint8_t _pinA;
  static volatile uint8_t _pinB;
  static volatile uint8_t _lastStateA;

  // Time-based debounce
  static volatile uint32_t _lastInterruptTime;
  static const uint32_t DEBOUNCE_US = 1000; // 0.5ms minimum between accepted edges

  static void IRAM_ATTR updateEncoder();

public:
  static volatile bool currentDir;   // true = CW, false = CCW
  static volatile int16_t counter;
  static void init(uint8_t pinA, uint8_t pinB);
};

// ---- Static member definitions ----

volatile uint8_t  AlexEncoder::_pinA              = 0;
volatile uint8_t  AlexEncoder::_pinB              = 0;
volatile uint8_t  AlexEncoder::_lastStateA        = 0;
volatile uint32_t AlexEncoder::_lastInterruptTime = 0;
volatile bool     AlexEncoder::currentDir         = true;
volatile int16_t  AlexEncoder::counter            = 0;

void IRAM_ATTR AlexEncoder::updateEncoder()
{
  uint32_t now = micros();

  // Time-based debounce: discard any edge that arrives too soon after the last one.
  if (now - _lastInterruptTime < DEBOUNCE_US)
    return;
  _lastInterruptTime = now;

  uint8_t curStateA = digitalRead(_pinA);

  // React only to rising edge of CLK (original counting logic — one count per cycle)
  if (curStateA != _lastStateA && curStateA == HIGH)
  {
    uint8_t stateB = digitalRead(_pinB);

    // CCW: DT != CLK
    if (stateB != curStateA)
    {
      counter--;
      currentDir = false;
    }
    // CW: DT == CLK
    else
    {
      counter++;
      currentDir = true;
    }
  }

  _lastStateA = curStateA;
}

void AlexEncoder::init(uint8_t pinA, uint8_t pinB)
{
  _pinA = pinA;
  _pinB = pinB;

  pinMode(_pinA, INPUT_PULLUP);
  pinMode(_pinB, INPUT_PULLUP);

  _lastStateA        = digitalRead(_pinA);
  _lastInterruptTime = 0;

  // Interrupt only on CLK (pinA). Pin B is read synchronously inside the ISR.
  attachInterrupt(digitalPinToInterrupt(_pinA), updateEncoder, CHANGE);
}