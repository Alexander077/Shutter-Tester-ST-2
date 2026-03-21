#pragma once

#include "Arduino.h"

class AlexEncoder
{
private:
  static uint8_t _pinA;
  static uint8_t _pinB;
  static uint8_t currentStateA;
  static uint8_t lastStateA;

  
  public:
  static bool currentDir;
  static int16_t counter;
  static void init(uint8_t, uint8_t);
  static void tick();
};

uint8_t  AlexEncoder::_pinA = 0;
uint8_t  AlexEncoder::_pinB = 0;
int16_t  AlexEncoder::counter = 0;
uint8_t AlexEncoder::currentStateA = 0;
uint8_t AlexEncoder::lastStateA = 0;
bool  AlexEncoder::currentDir = true;

void AlexEncoder::tick()
{
  // Читаем текущее состояние пина A
  currentStateA = digitalRead(_pinA);

  // Реагируем только в момент изменения состояния пина A
  if (currentStateA != lastStateA)
  {

    // Считаем только по спадающему фронту (когда пин замыкается на землю)
    // Это предотвращает двойной счет на один физический клик энкодера
    if (currentStateA == LOW)
    {

      // Если состояние пина B отличается от пина A, крутим в одну сторону
      if (digitalRead(_pinB) == HIGH)
      {
        counter--;         // Увеличиваем счетчик
        currentDir = false; // Направление по часовой стрелке[cite: 1]
      }
      else
      {
        counter++;          // Уменьшаем счетчик[cite: 1]
        currentDir = true; // Направление против часовой стрелки[cite: 1]
      }
    }
  }

  // Обновляем предыдущее состояние для следующего тика[cite: 1]
  lastStateA = currentStateA;
}

void AlexEncoder::init(uint8_t pinA, uint8_t pinB)
{
  AlexEncoder::_pinA = pinA;
  AlexEncoder::_pinB = pinB;

  pinMode(AlexEncoder::_pinA, INPUT_PULLUP);
  pinMode(AlexEncoder::_pinB, INPUT_PULLUP);
}

