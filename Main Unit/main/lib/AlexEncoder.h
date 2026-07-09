#pragma once

#include "Arduino.h"

class AlexEncoder
{
private:
  static volatile uint8_t  _pinA;
  static volatile uint8_t  _pinB;
  static volatile uint8_t  _prevState;          // Previous 2-bit state {CLK, DT}
  static volatile int8_t   _quadAccumulator;    // Accumulates ±1 per valid quadrature edge
  static volatile uint32_t _lastInterruptTime;
  static const uint32_t    DEBOUNCE_US = 2000;  // 5ms — sufficient for mechanical encoders

  // Quadrature lookup table: index = (prev_state << 2) | new_state
  // Return: -1 = valid CCW step, +1 = valid CW step, 0 = invalid/bounce (ignore)
  static const int8_t TRANSITIONS[16];

  static void IRAM_ATTR onPinChange();

  static const int8_t QUAD_STEPS_PER_DETENT = 4;  // 4 quadrature edges per mechanical detent
  static const int8_t DETENT_THRESHOLD_CW  =  QUAD_STEPS_PER_DETENT;
  static const int8_t DETENT_THRESHOLD_CCW = -QUAD_STEPS_PER_DETENT;

public:
  static volatile bool    currentDir;   // true = CW, false = CCW
  static volatile int16_t counter;
  static void init(uint8_t pinA, uint8_t pinB);
};

// ---- Static member definitions ----

volatile uint8_t  AlexEncoder::_pinA              = 0;
volatile uint8_t  AlexEncoder::_pinB              = 0;
volatile uint8_t  AlexEncoder::_prevState         = 0;
volatile int8_t   AlexEncoder::_quadAccumulator   = 0;
volatile uint32_t AlexEncoder::_lastInterruptTime = 0;
volatile bool     AlexEncoder::currentDir         = true;
volatile int16_t  AlexEncoder::counter            = 0;

// Gray-code quadrature transition table.
// Rows: previous state {CLK,DT} (0–3), Cols: new state {CLK,DT} (0–3)
//
// Valid sequences for a standard mechanical quadrature encoder:
//   CW  (clockwise):        00 → 10 → 11 → 01 → 00 ...
//   CCW (counter-clockwise): 00 → 01 → 11 → 10 → 00 ...
//
// Any other transition is contact bounce or noise → returns 0.
const int8_t AlexEncoder::TRANSITIONS[16] =
{
  // prev=00 (0):          new=00   new=01   new=10   new=11
  /*                     */    0,       1,      -1,       0,
  // prev=01 (1):          new=00   new=01   new=10   new=11
  /*                     */   -1,       0,       0,       1,
  // prev=10 (2):          new=00   new=01   new=10   new=11
  /*                     */    1,       0,       0,      -1,
  // prev=11 (3):          new=00   new=01   new=10   new=11
  /*                     */    0,      -1,       1,       0
};

void IRAM_ATTR AlexEncoder::onPinChange()
{
  uint32_t now = micros();

  // Time-based debounce guard: reject edges that arrive too soon after the last one
  if (now - _lastInterruptTime < DEBOUNCE_US)
    return;
  _lastInterruptTime = now;

  // Build current 2-bit state: bit1 = CLK (pinA), bit0 = DT (pinB)
  uint8_t newState = (digitalRead(_pinA) ? 2 : 0) | (digitalRead(_pinB) ? 1 : 0);

  // Lookup the transition validity & direction
  int8_t step = TRANSITIONS[(_prevState << 2) | newState];

  if (step != 0)
  {
    // Accumulate quadrature edges. Bounce edges sum to 0 (e.g. +1 then -1 = 0).
    _quadAccumulator += step;

    // Commit 1 public counter tick per full mechanical detent (4 quadrature edges)
    if (_quadAccumulator >= DETENT_THRESHOLD_CW)
    {
      counter++;
      currentDir = true;
      _quadAccumulator -= QUAD_STEPS_PER_DETENT;
    }
    else if (_quadAccumulator <= DETENT_THRESHOLD_CCW)
    {
      counter--;
      currentDir = false;
      _quadAccumulator += QUAD_STEPS_PER_DETENT;
    }
  }

  _prevState = newState;
}

void AlexEncoder::init(uint8_t pinA, uint8_t pinB)
{
  _pinA = pinA;
  _pinB = pinB;

  pinMode(_pinA, INPUT_PULLUP);
  pinMode(_pinB, INPUT_PULLUP);

  // Read initial state after pullups settle
  _prevState        = (digitalRead(_pinA) ? 2 : 0) | (digitalRead(_pinB) ? 1 : 0);
  _quadAccumulator  = 0;
  _lastInterruptTime = 0;

  // Interrupt on CHANGE for both pins to capture every quadrature edge
  attachInterrupt(digitalPinToInterrupt(_pinA), onPinChange, CHANGE);
  attachInterrupt(digitalPinToInterrupt(_pinB), onPinChange, CHANGE);
}