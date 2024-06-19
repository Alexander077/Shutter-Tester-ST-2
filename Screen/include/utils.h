#pragma once

#include <Arduino.h>

void substring(const char *source, char *destination, int start, int length)
{
  strncpy(destination, source + start, length);
  destination[length] = '\0';
}