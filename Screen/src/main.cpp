#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <U8g2lib.h>
#include "credits.h"
#include "utils.h"

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

enum class Screens
{
  MAIN_MENU = 'm',
  MEASURING = 'n',
  MEASURED = 'd',
  CREDITS = 'c',
  TEST = 't',
};

struct Rect
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
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

String inputCmdStr = "m:1";
char curScreen = '\0';
char prevMode = '\0';

void receiveEvent(int howMany)
{
  char inputCmdStrBuf[100];
  inputCmdStrBuf[0] = '\0';

  while (Wire.available()) 
  {
    char c = Wire.read(); // receive byte as a character
    char tmpBuf[2] = "\0";
    tmpBuf[0] = c;
    strcat(inputCmdStrBuf, tmpBuf);
  }

  inputCmdStr = String(inputCmdStrBuf);
}

Rect getStringRect(const char *str, int16_t x, int16_t y)
{
  // Variables to hold the bounding box dimensions
  int16_t x1, y1;
  uint16_t w, h;

  // Get the bounds of the text
  display->getTextBounds(str, x, y, &x1, &y1, &w, &h);

  Rect r;
  r.x = x1;
  r.y = y1;
  r.width = w;
  r.height = h;
  return r;
}

Rect drawStringHCentered(const char *str, uint16_t y)
{
  // // Variables to hold the bounding box dimensions
  // int16_t x1, y1;
  // uint16_t w, h;

  Rect r = getStringRect(str, 0, y);
  // Serial.println(r.y);

  // Calculate the position to center the text on the screen
  int16_t x = (display->width() - r.width) / 2;
  // int16_t y = (display->height() - h) / 2;
  r.x = x;

  // Draw the text at the calculated position
  display->setCursor(x, y);
  display->print(str);

  // Serial.print(r.x);
  // Serial.print("|");
  // Serial.print(r.y);
  // Serial.print("|");
  // Serial.print(r.width);
  // Serial.print("|");
  // Serial.println(r.height);

  // Optional: Draw the bounding box around the text for visualization
  // display->drawRect(x, y - r.height, r.width, r.height, RED);
  return r;
}

Rect drawStringHCentered(String str, uint16_t y)
{
  #define CHAR_BUF_SIZE 50
  char buf[CHAR_BUF_SIZE];
  str.toCharArray(buf, CHAR_BUF_SIZE );
  return drawStringHCentered(buf, y);
}

char getCurScreen()
{
  return inputCmdStr[0];
}

void drawMainMenu()
{
  Rect menuItemsRects[MAIN_MENU_ITEMS_COUNT];
  int16_t menuItemsY[MAIN_MENU_ITEMS_COUNT];

  // if (prevMode != inputCmdStr[0])
  // {
    display->fillScreen(BLACK);

    for (uint8_t i = 0, y = 45; i < MAIN_MENU_ITEMS_COUNT; i++, y += 15)
    {
      menuItemsRects[i] = drawStringHCentered(GetMenuItemTitle((MainMenuItems)i), y);
      menuItemsY[i] = y;
    }
    // prevMode = inputCmdStr[0];
  // }

  int8_t selectedMenuItemIndex = 0;
  int8_t prevSelectedMenuItemIndex = -1;

  while ((Screens)getCurScreen() == Screens::MAIN_MENU)
  {
    String selIndex = inputCmdStr.substring(2);
    selectedMenuItemIndex = selIndex.toInt();

    // if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
    // {
    //   Serial.print(prevSelectedMenuItemIndex);
    //   Serial.print(":");
    //   Serial.println(selectedMenuItemIndex);
    // }

    if (prevSelectedMenuItemIndex != selectedMenuItemIndex)
    {
      // Serial.println(selectedMenuItemIndex);
      // Serial.print(prevSelectedMenuItemIndex);
      // Serial.print(":");
      // Serial.println(selectedMenuItemIndex);

      // Serial.println(selectedMenuItemIndex);

      if (prevSelectedMenuItemIndex != -1)
      {
        Rect rOld = menuItemsRects[prevSelectedMenuItemIndex];
        display->fillRect(rOld.x, rOld.y /* - rOld.height */, rOld.width, rOld.height, BLACK);
        menuItemsRects[prevSelectedMenuItemIndex] =
            drawStringHCentered(GetMenuItemTitle((MainMenuItems)prevSelectedMenuItemIndex), menuItemsY[prevSelectedMenuItemIndex]);
      }

      String newSelMenuItem = "-> " + String(GetMenuItemTitle((MainMenuItems)selectedMenuItemIndex)) + " <-";
      Rect rNew = menuItemsRects[selectedMenuItemIndex];
      // Serial.println(rNew.y);
      display->fillRect(rNew.x, rNew.y /* - rNew.height */, rNew.width, rNew.height, BLACK);

      menuItemsRects[selectedMenuItemIndex] = drawStringHCentered(newSelMenuItem, menuItemsY[selectedMenuItemIndex]);
      prevSelectedMenuItemIndex = selectedMenuItemIndex;
      // Serial.println(newSelMenuItem);
      // display->setCursor(10, 10);
      // display->drawChar(10, 10, selectedMenuItemIndexStrBuf[0], WHITE, BLACK);
      // display->print(selectedMenuItemIndex);
      // display->println(Credits::ADAFRUIT_GFX_LICENSE_HEADER);
    }
  }
}

void drawCreditsScreen()
{
  display->fillScreen(BLACK);

  while ((Screens)getCurScreen() == Screens::CREDITS)
  {
    // display->setFont();
    display->setCursor(0, 13);
    display->print(Credits::CREDITS_HEADER);
  }
}

void drawMeasuringScreen()
{
  display->fillScreen(BLACK);

  while ((Screens)getCurScreen() == Screens::MEASURING)
  {
    // display->setFont();
    display->setTextSize(2);
    drawStringHCentered("MEASURING", 40);
    display->setTextSize(1);
    drawStringHCentered("Release camera shutter", 60);
    // display->setCursor(0, 13);
  }
}

void drawMeasuredScreen()
{
  display->fillScreen(BLACK);

  while ((Screens)getCurScreen() == Screens::MEASURED)
  {
    // display->setFont();
    drawStringHCentered("Sensor 1 results", 20);
    // display->setCursor(0, 13);
  }
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
  // display->setTextSize(1,1,10);
}

void loop()
{

  while (true)
  {
    curScreen = getCurScreen();
    // Serial.println(inputCmdStr); // print the character

    switch ((Screens)curScreen)
    {
      case Screens::MAIN_MENU:
      {
        drawMainMenu();
        break;
      }
      case Screens::MEASURING:
      {
        drawMeasuringScreen();
        break;
      }
      case Screens::MEASURED:
      {
        drawMeasuredScreen();
        break;
      }
      case Screens::CREDITS:
      {
        drawCreditsScreen();
        break;
      }

      default:
        break;
    }
  }
}