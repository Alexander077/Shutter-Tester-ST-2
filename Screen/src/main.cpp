#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "credits.h"

#define TFT_CS 5     
#define TFT_RESET 13 
#define TFT_DC 12    
// #define TFT_MOSI 10 
// #define TFT_SCK 8   
// #define TFT_MISO -1 


/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *gfx = new Arduino_ST7735(
    bus, TFT_RESET, 3 /* rotation */, false /* IPS */,
    128 /* width */, 160 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */,
    0 /* col offset 2 */, 0 /* row offset 2 */,
    false /* BGR */);

String s = "";

// function that executes whenever data is received from master
// this function is registered as an event, see setup()
void receiveEvent(int howMany)
{
  s.clear();

  while (1 < Wire.available()) // loop through all but the last
  {
    char c = Wire.read(); // receive byte as a character
    s.concat(c);
    // Serial.print(c);      // print the character
  }
  int x = Wire.read(); // receive byte as an integer
  // Serial.println(x);   // print the integer
}

void setup(void)
{
  Wire.begin(4);                // join i2c bus with address #4
  Wire.onReceive(receiveEvent); // register event

  gfx->begin();
  gfx->fillScreen(BLACK);

  gfx->setTextColor(WHITE);
  gfx->setTextSize(1, 1, 5);

 

  // gfx->setCursor(10, 30);
  // gfx->println("+ ILI9488 SPI TFT");

  // gfx->setCursor(10, 50);
  // gfx->println("using Arduino_GFX Library");

  // int w = gfx->width();
  // int h = gfx->height();

  // gfx->setCursor(10, 70);
  // gfx->printf("%i x %d", w, h);
  // gfx->drawRect(0, 0, w, h, WHITE);

  // delay(3000);

  // for (int i = 0; i < w; i++)
  // {
  //   int d = (int)(255 * i / w);
  //   gfx->drawLine(i, 0, i, h, RGB565(d, 0, 0));
  //   // delay(10);
  // }
  // delay(3000);

  // for (int i = 0; i < w; i++)
  // {
  //   int d = (int)(255 * i / w);
  //   gfx->drawLine(w - i, 0, w - i, h, RGB565(0, d, 0));
  //   // delay(10);
  // }
  // delay(3000);

  // for (int i = 0; i < w; i++)
  // {
  //   int d = (int)(255 * i / w);
  //   gfx->drawLine(i, 0, i, h, RGB565(0, 0, d));
  //   // delay(10);
  // }
  // delay(3000);
}

void loop()
{
  gfx->setCursor(10, 10);
  gfx->fillScreen(BLACK);
  // gfx->println(Credits::ADAFRUIT_GFX_LICENSE_HEADER);
  gfx->println(s);
  delay(500);

  // gfx->setTextColor(WHITE);
  // gfx->setTextSize(2, 2, 2);

  // gfx->fillScreen(RED);
  // gfx->setCursor(100, 100);
  // gfx->printf("RED");
  // delay(2000);

  // gfx->fillScreen(GREEN);
  // gfx->setCursor(100, 100);
  // gfx->printf("GREEN");
  // delay(2000);

  // gfx->fillScreen(BLUE);
  // gfx->setCursor(100, 100);
  // gfx->printf("BLUE");
  // delay(2000);
}