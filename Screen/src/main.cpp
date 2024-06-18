#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include "credits.h"

#define TFT_CS 5     
#define TFT_RESET 13 
#define TFT_DC 12    
// #define TFT_MOSI 10 
// #define TFT_SCK 8   
// #define TFT_MISO -1

#define DISPLAY_I2C_ADDRESS 4

#define MAIN_MENU_ITEMS_COUNT 4

enum class MainMenuItems
{
  MEASURE,
  CHECK_LIGHT,
  MEASURMENT_HISTORY,
  CREDITS,
};

struct Rect
{
  uint16_t x;
  uint16_t y;
  uint16_t width;
  uint16_t height;
};

const char *MainMenuItemsStr[] =
    {
        "Measure",
        "Check Light",
        "View Last Measures",
        "Credits"};

const char *GetMenuItemTitle(MainMenuItems menuItem)
{
  return MainMenuItemsStr[(uint8_t)menuItem];
}

/* More data bus class: https://github.com/moononournation/Arduino_GFX/wiki/Data-Bus-Class */
Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);

/* More display class: https://github.com/moononournation/Arduino_GFX/wiki/Display-Class */
Arduino_GFX *display = new Arduino_ST7735(
    bus, TFT_RESET, 3 /* rotation */, false /* IPS */,
    128 /* width */, 160 /* height */,
    0 /* col offset 1 */, 0 /* row offset 1 */,
    0 /* col offset 2 */, 0 /* row offset 2 */,
    false /* BGR */);

char inputCmdStr[100] = "m:1\0";

void substring(const char *source, char *destination, int start, int length)
{
  strncpy(destination, source + start, length);
  destination[length] = '\0'; 
}

void receiveEvent(int howMany)
{
  inputCmdStr[0] = '\0';

  while (Wire.available()) // loop through all but the last
  {
    char c = Wire.read(); // receive byte as a character
    char tmpBuf[2] = "\0";
    tmpBuf[0] = c;
    strcat(inputCmdStr, tmpBuf);
  }
}

Rect getStringRect(const char *str)
{
  // Variables to hold the bounding box dimensions
  int16_t x1, y1;
  uint16_t w, h;

  // Get the bounds of the text
  display->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);

  Rect r;
  r.x = x1;
  r.y = y1;
  r.width = w;
  r.height = h;
  return r;
}

Rect drawStringCentered(const char *str, uint16_t y)
{
  // // Variables to hold the bounding box dimensions
  // int16_t x1, y1;
  // uint16_t w, h;

  // // Get the bounds of the text
  // display->getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  Rect r = getStringRect(str);

  // Calculate the position to center the text on the screen
  int16_t x = (display->width() - r.width) / 2;
  // int16_t y = (display->height() - h) / 2;

  // Draw the text at the calculated position
  display->setCursor(x, y);
  display->print(str);

  // Optional: Draw the bounding box around the text for visualization
  // display->drawRect(x, y, r.width, r.height, RED);
  return r;
}

Rect drawStringCentered(String str, uint16_t y)
{
  #define CHAR_BUF_SIZE 50
  char buf[CHAR_BUF_SIZE];
  str.toCharArray(buf, CHAR_BUF_SIZE );
  return drawStringCentered(buf, y);
}



void setup(void)
{
  Serial.begin(115200);

  Wire.begin(DISPLAY_I2C_ADDRESS); // join i2c bus with address #4
  Wire.onReceive(receiveEvent); // register event

  display->begin();
  display->fillScreen(BLACK);

  display->setTextColor(WHITE);
  display->setFont(u8g2_font_6x13_tf);
  display->setTextSize(1);

   // display->setCursor(10, 30);
  // display->println("+ ILI9488 SPI TFT");

  // display->setCursor(10, 50);
  // display->println("using Arduino_GFX Library");

  // int w = display->width();
  // int h = display->height();

  // display->setCursor(10, 70);
  // display->printf("%i x %d", w, h);
  // display->drawRect(0, 0, w, h, WHITE);

  // delay(3000);

  // for (int i = 0; i < w; i++)
  // {
  //   int d = (int)(255 * i / w);
  //   display->drawLine(i, 0, i, h, RGB565(d, 0, 0));
  //   // delay(10);
  // }
  // delay(3000);

  // for (int i = 0; i < w; i++)
  // {
  //   int d = (int)(255 * i / w);
  //   display->drawLine(w - i, 0, w - i, h, RGB565(0, d, 0));
  //   // delay(10);
  // }
  // delay(3000);

  // for (int i = 0; i < w; i++)
  // {
  //   int d = (int)(255 * i / w);
  //   display->drawLine(i, 0, i, h, RGB565(0, 0, d));
  //   // delay(10);
  // }
  // delay(3000);
}

void loop()
{
  char prevMode = '\0';

  while (true)
  {
    // Serial.println(inputCmdStr); // print the character

    switch (inputCmdStr[0])
    {
      case 'm':
      {
        static Rect menuItemsRects[MAIN_MENU_ITEMS_COUNT];

        if (prevMode != inputCmdStr[0])
        {
          display->fillScreen(BLACK);

          for (uint8_t i = 0, y = 45; i < MAIN_MENU_ITEMS_COUNT; i++, y += 13)
          {
            menuItemsRects[i] = drawStringCentered(GetMenuItemTitle((MainMenuItems)i), y);
          }
          prevMode = inputCmdStr[0];
        }

        static int8_t selectedMenuItemIndex = 0;
        char selectedMenuItemIndexStrBuf[5] = "\0\0\0\0";
        substring(inputCmdStr, selectedMenuItemIndexStrBuf, 2, 3);
        sscanf(selectedMenuItemIndexStrBuf, "%d", &selectedMenuItemIndex);
        // Serial.println(selectedMenuItemIndex);
        static int8_t prevSelectedMenuItemIndex = -1;

        if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
        {
          Serial.print(prevSelectedMenuItemIndex);
          Serial.print(":");
          Serial.println(selectedMenuItemIndex);
        }


        if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
        {
          // Serial.println(selectedMenuItemIndex);

          // Serial.println(selectedMenuItemIndex);

          if (prevSelectedMenuItemIndex != -1)
          {
            Rect rOld = menuItemsRects[prevSelectedMenuItemIndex];
            display->fillRect(rOld.x, rOld.y - rOld.height, rOld.width, rOld.height, BLACK);
          }

          String newSelMenuItem = ">" + String(GetMenuItemTitle((MainMenuItems)selectedMenuItemIndex)) + "<";
          Rect rNew = menuItemsRects[selectedMenuItemIndex];
          Serial.println(rNew.y);
          display->fillRect(rNew.x, rNew.y - rNew.height, rNew.width, rNew.height, BLACK);

          menuItemsRects[selectedMenuItemIndex] = drawStringCentered(newSelMenuItem, rNew.y);
          prevSelectedMenuItemIndex = selectedMenuItemIndex;
          // Serial.println(newSelMenuItem);
          // display->setCursor(10, 10);
          // display->drawChar(10, 10, selectedMenuItemIndexStrBuf[0], WHITE, BLACK);
          // display->print(selectedMenuItemIndex);
          // display->println(Credits::ADAFRUIT_GFX_LICENSE_HEADER);
        }
        
        break;
      }
      
      default:
        break;
    }
  }
  

  // display->setCursor(10, 10);
  // display->fillScreen(BLACK);
  // // display->println(Credits::ADAFRUIT_GFX_LICENSE_HEADER);
  // display->println(inputCmdStr);
  // delay(50);

  // display->setTextColor(WHITE);
  // display->setTextSize(2, 2, 2);

  // display->fillScreen(RED);
  // display->setCursor(100, 100);
  // display->printf("RED");
  // delay(2000);

  // display->fillScreen(GREEN);
  // display->setCursor(100, 100);
  // display->printf("GREEN");
  // delay(2000);

  // display->fillScreen(BLUE);
  // display->setCursor(100, 100);
  // display->printf("BLUE");
  // delay(2000);
}