#pragma once

#include "Arduino.h"
#include "Common.h"
#include "driver/gpio.h"
#include "esp_timer.h"

class AlexEncoder
{
private:
  static uint8_t  _pinA;              // CLK pin
  static uint8_t  _pinB;              // DT pin
  static uint8_t  _prevState;         // Previous 2-bit state {CLK, DT}
  static int8_t   _quadAccumulator;   // Accumulates ±1 per valid quadrature edge
  static esp_timer_handle_t _pollTimer;

  // Quadrature lookup table: index = (prev_state << 2) | new_state
  // Return: -1 = valid CCW step, +1 = valid CW step, 0 = invalid/bounce (ignore)
  static const int8_t TRANSITIONS[16];

  static const int8_t QUAD_STEPS_PER_DETENT = 4;  // 4 quadrature edges per mechanical detent

  static void onPollTimer(void *arg);

public:
  static volatile bool    currentDir;   // true = CW, false = CCW
  static volatile int16_t counter;
  static void init(uint8_t pinA, uint8_t pinB);
};

// ---- Static member definitions ----

uint8_t  AlexEncoder::_pinA            = 0;
uint8_t  AlexEncoder::_pinB            = 0;
uint8_t  AlexEncoder::_prevState       = 0;
int8_t   AlexEncoder::_quadAccumulator = 0;
esp_timer_handle_t AlexEncoder::_pollTimer = nullptr;
volatile bool    AlexEncoder::currentDir = true;
volatile int16_t AlexEncoder::counter    = 0;

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

// Polling-based quadrature decoding
void AlexEncoder::onPollTimer(void *arg)
{
  // Sample both pins: bit1 = CLK (pinA), bit0 = DT (pinB)
  uint8_t newState = (gpio_get_level((gpio_num_t)_pinA) ? 2 : 0) |
                     (gpio_get_level((gpio_num_t)_pinB) ? 1 : 0);

  int8_t step = TRANSITIONS[(_prevState << 2) | newState];
  _prevState = newState;

  if (step == 0)
    return;

  _quadAccumulator += step;

  if (_quadAccumulator >= QUAD_STEPS_PER_DETENT)
  {
    counter++;
    currentDir = true;
    _quadAccumulator = 0;
  }
  else if (_quadAccumulator <= -QUAD_STEPS_PER_DETENT)
  {
    counter--;
    currentDir = false;
    _quadAccumulator = 0;
  }
  else
  {
    return; // Partial step — nothing committed yet
  }

  // Debug: one TEST_PIN_1 toggle per committed detent click
  static bool pinState = HIGH;
  digitalWrite(TEST_PIN_1, pinState);
  pinState = !pinState;
}

void AlexEncoder::init(uint8_t pinA, uint8_t pinB)
{
  _pinA = pinA;
  _pinB = pinB;

  pinMode(_pinA, INPUT);
  pinMode(_pinB, INPUT);

  // Read initial state after pins settle
  _prevState = (gpio_get_level((gpio_num_t)_pinA) ? 2 : 0) |
               (gpio_get_level((gpio_num_t)_pinB) ? 1 : 0);
  _quadAccumulator = 0;

  // Poll pin levels with a 1 ms periodic timer instead of GPIO interrupts
  const esp_timer_create_args_t timerArgs = {
    .callback = &AlexEncoder::onPollTimer,
    .arg = NULL,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "encoder_poll",
    .skip_unhandled_events = true
  };

  esp_timer_create(&timerArgs, &_pollTimer);
  esp_timer_start_periodic(_pollTimer, 1000); // 1000 us = 1 ms
}
