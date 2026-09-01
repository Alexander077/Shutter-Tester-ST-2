#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define DISPLAY_CS SS			// 5
#define DISPLAY_SCK SCK		// 18
#define DISPLAY_MOSI MOSI // 23
#define DISPLAY_RESET 17
#define DISPLAY_DC 16

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
// Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_DataBus *bus = new Arduino_HWSPI(DISPLAY_DC, DISPLAY_CS, DISPLAY_SCK, DISPLAY_MOSI);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *display = new Arduino_ST7735(
		bus, DISPLAY_RESET, 1 /* rotation */, false /* IPS */,
		128 /* width */, 160 /* height */,
		0 /* col offset 1 */, 0 /* row offset 1 */,
		0 /* col offset 2 */, 0 /* row offset 2 */,
		false /* BGR */);