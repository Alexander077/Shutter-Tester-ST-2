#pragma once

#define RESULT_PAGES_COUNT 4
#define MIN_ALLOWED_SIGNAL_LEVEL 95
#define MAX_ALLOWED_SIGNAL_LEVEL 245
#define MAX_SIGNAL_LEVEL 248

enum class Screens
{
  MAIN_MENU = 'm',
  MEASURING = 'n',
  MEASURED = 'r',
  CREDITS = 'c',
  CURTAN_MOVEMENT_SELECTION = 'd',
  LIGHT_CHECK = 'l',
  TEST = 't',
};