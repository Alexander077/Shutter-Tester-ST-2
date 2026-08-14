#pragma once

#include "Arduino.h"
#include "Common.h"

class AlexEncoder
{
private:
  static volatile uint8_t  _pinA;               // CLK pin (interrupt source)
  static volatile uint8_t  _pinB;               // DT pin (direction sense)
  static volatile uint32_t _lastInterruptTime;
  static const uint32_t    DEBOUNCE_US = 2000;  // 5 ms — sufficient for mechanical encoders

  static void IRAM_ATTR onPinChange();

public:
  static volatile bool    currentDir;   // true = CW, false = CCW
  static volatile int16_t counter;
  static void init(uint8_t pinA, uint8_t pinB);
};

// ---- Static member definitions ----

volatile uint8_t  AlexEncoder::_pinA              = 0;
volatile uint8_t  AlexEncoder::_pinB              = 0;
volatile uint32_t AlexEncoder::_lastInterruptTime = 0;
volatile bool     AlexEncoder::currentDir         = true;
volatile int16_t  AlexEncoder::counter            = 0;

// Classic algorithm: one interrupt per detent on CLK falling edge.
// DT level sampled inside ISR determines direction.
void IRAM_ATTR AlexEncoder::onPinChange()
{
  static bool pinState = HIGH;
  digitalWrite(TEST_PIN_1, pinState);
  pinState = !pinState;

  uint32_t now = micros();

  // Time-based debounce guard: reject edges that arrive too soon after the last one
  if (now - _lastInterruptTime < DEBOUNCE_US)
    return;
  _lastInterruptTime = now;

  // Sample DT (pinB) at the moment CLK falls
  // LOW  = CW  (clockwise)
  // HIGH = CCW (counter-clockwise)
  if (digitalRead(_pinB) == LOW)
  {
    counter++;
    currentDir = true;
  }
  else
  {
    counter--;
    currentDir = false;
  }
}

void AlexEncoder::init(uint8_t pinA, uint8_t pinB)
{
  _pinA = pinA;
  _pinB = pinB;

  pinMode(_pinA, INPUT);
  pinMode(_pinB, INPUT);

  _lastInterruptTime = 0;

  // Interrupt only on CLK (pinA) falling edge — one interrupt per mechanical detent
  attachInterrupt(digitalPinToInterrupt(_pinA), onPinChange, FALLING);
}
